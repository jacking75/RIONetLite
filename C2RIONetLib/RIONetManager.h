#pragma once

#include <vector>
#include <thread>
#include <concurrent_queue.h>

#include "SessionManager.h"
#include "CommonDef.h"


namespace C2RIONetLib
{	
	template<class TNetEventManager>
	class RIONetManager
	{
	public:
		RIONetManager() = default;
		~RIONetManager() = default;
		
		NetErrorCode Initialize(NetConfig netConfig, TNetEventManager* tNetEventManager)
		{
			if ((netConfig.MaxConnectionCount % netConfig.IoWorkThreadCount) != 0) { return NetErrorCode::session_count_per_thread_incorrect; }

			auto sessionCountPerRioThread = netConfig.MaxConnectionCount / netConfig.IoWorkThreadCount;
			mNetConfig = netConfig;
			auto rioThreadCount = netConfig.IoWorkThreadCount;
			mNetEventManager = tNetEventManager;

			/// winsock initializing			
			if (WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { return NetErrorCode::WSAStartUp_fail; }

			/// create TCP socket with RIO mode
			mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_REGISTERED_IO);
			if (mListenSocket == INVALID_SOCKET) { return NetErrorCode::listen_socket_create_fail; }

			int opt = 1;
			setsockopt(mListenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(int));

			/// bind
			SOCKADDR_IN serveraddr;
			ZeroMemory(&serveraddr, sizeof(serveraddr));
			serveraddr.sin_family = AF_INET;
			serveraddr.sin_port = htons(netConfig.PortNumber);
			serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

			if (SOCKET_ERROR == bind(mListenSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr))) { return NetErrorCode::listen_socket_bind_fail; }

			if (RIOAPI::SetFunction(mListenSocket) == false) { return NetErrorCode::rio_function_init_fail; }
			
			if (mSessionManager.PrepareSessionPool(sessionCountPerRioThread, netConfig) == false) { return NetErrorCode::prepare_session_pool_fail; }

			/// listen
			if (SOCKET_ERROR == listen(mListenSocket, SOMAXCONN)) { return NetErrorCode::listen_socket_listen_fail; }

			const unsigned int maxCQSizePerRIOThread = (netConfig.Conn.MAXRecvCountPer1Sec + netConfig.Conn.MAXSendCountPer1Sec) * sessionCountPerRioThread;
			StartIoThreads(rioThreadCount, maxCQSizePerRIOThread);

			LogFuncPtr((int)LogLevel::info, "RIONetManager::Initialize() - Success!");
			return NetErrorCode::none;
		}

		void StartAndBlocking()
		{			
			StartAcceptLoop(mNetConfig.Conn);
		}						

		void End()
		{
			LogFuncPtr((int)LogLevel::info, "RIONetManager::End - Start");

			LogFuncPtr((int)LogLevel::info, "RIONetManager::End - Close mListenSocket");
			mIsRunAcceptThread = false;
			closesocket(mListenSocket);
			
			LogFuncPtr((int)LogLevel::info, "RIONetManager::End - Stop IOThreads");
			mIsRunIOWorkThread = false;
			for (auto& thread : mIOWorkThreads)
			{
				if (thread.joinable())
				{
					thread.join();
				}
			}
			

			mSessionManager.Dispose();

			WSACleanup();

			LogFuncPtr((int)LogLevel::info, "RIONetManager::End - Complete");
		}

		const RIO_CQ& GetCompletionQueue(int threadId) { return mRioCompletionQueues[threadId]; }

		NetErrorCode SendData(const INT32 sessionIndex, const UINT64 sessionUniqueId, const UINT32 dataSize, const char* pData)
		{
			auto session = mSessionManager.GetSessionObj(sessionIndex, sessionUniqueId);
			if (session == nullptr)
			{
				return NetErrorCode::send_invalid_session;
			}

			if (session->IsEnableNetwork() == false)
			{
				return NetErrorCode::send_session_network_disable;
			}

			if (session->PostSend(dataSize, pData) == false)
			{				
				ForcedClose(session, DisconnectReason::send_error);
				return NetErrorCode::send_io_fail;
			}

			return NetErrorCode::none;
		}

		void ForcedClose(const INT32 sessionIndex, const UINT64 sessionUniqueId)
		{
			auto session = mSessionManager.GetSessionObj(sessionIndex, sessionUniqueId);
			if (session == nullptr)
			{
				return;
			}

			session->SetDisableNetwork();

			auto threadIndex = session->GetRioThreadId();
			LazyedDisConnectSessions[threadIndex].push({ sessionIndex, sessionUniqueId, DisconnectReason::forces_close_server });
		}

	private:	
		void ForcedClose(Session* pSession, DisconnectReason reason)
		{
			pSession->SetDisableNetwork();

			auto threadIndex = pSession->GetRioThreadId();
			auto sessionIndex = pSession->GetIndex();
			auto sessionUniqueId = pSession->GetUniqueId();

			LazyedDisConnectSessions[threadIndex].push({ sessionIndex, sessionUniqueId, reason });
		}

		void StartAcceptLoop(ConnectionNetConfig netConfig)
		{
			mIsRunAcceptThread = true;
			char logmsg[256] = { 0, };

			while (mIsRunAcceptThread)
			{
				SOCKET acceptedSock = accept(mListenSocket, NULL, NULL);
				if (acceptedSock == INVALID_SOCKET)
				{
					LogFuncPtr((int)LogLevel::error, "RIONetManager::StartAcceptLoop() - accept invalid socket");
					continue;
				}

				auto curTimeSec = Helper::CurrentSecond();

				/// new client session (should not be under any session locks)
				auto pSession = mSessionManager.IssueClientSession(curTimeSec);
				if (pSession == nullptr)
				{			
					//사용할 수 있는 세션이 없는 상태이므로 연결을 끊고, 잠시 대기한다
					LogFuncPtr((int)LogLevel::debug, "RIONetManager::StartAcceptLoop() - Empry Session Pool");
					closesocket(acceptedSock);
					Sleep(1000);
					continue;					
				}

				auto clientThreadId = pSession->GetRioThreadId();

				/// connection establishing and then issuing recv
				/// create socket RQ
				/// SEND and RECV within one CQ (you can do with two CQs, seperately)
				auto rioRequestQueue = RIO.RIOCreateRequestQueue(acceptedSock,
					1, 1, // 받기는 연속으로 요청하지 않으므로 1
					netConfig.MAXSendCountPer1Sec, 1,
					GetCompletionQueue(clientThreadId),
					GetCompletionQueue(clientThreadId),
					NULL);
				if (rioRequestQueue == RIO_INVALID_RQ)
				{
					sprintf_s(logmsg, "RIONetManager::StartAcceptLoop() - RIOCreateRequestQueue Error: %d", GetLastError());
					LogFuncPtr((int)LogLevel::error, logmsg);

					Disconnect(pSession, 0, DisconnectReason::accept_loop_request_queue_error);
					continue;
				}

				++mConnectSequenceId;
				if (pSession->OnConnect(mConnectSequenceId, acceptedSock, rioRequestQueue) == false)
				{
					Disconnect(pSession, 0, DisconnectReason::accept_loop_on_connect_rrror);
					continue;
				}

				mNetEventManager->OnConnect(pSession->GetIndex(), pSession->GetUniqueId());
			}
		}

		bool CreateRioCompletionQueue(const unsigned int maxCQSizePerRIOThread)
		{
			auto rioQueue = RIO.RIOCreateCompletionQueue(maxCQSizePerRIOThread, 0);
			if (rioQueue == RIO_INVALID_CQ) { return false; }
			
			mRioCompletionQueues.push_back(rioQueue);
			return true;
		}

		void StartIoThreads(unsigned int rioThreadCount, const unsigned int maxCQSizePerRIOThread)
		{
			for (decltype(rioThreadCount) i = 0; i < rioThreadCount; ++i)
			{
				CreateRioCompletionQueue(maxCQSizePerRIOThread);
				mIOWorkThreads.emplace_back([&]() { IoWorkerThread(i); });
			}
		}

		void IoWorkerThread(const int _thread_index)
		{
			auto threadIndex = _thread_index;						
			auto coreCount = Helper::GetNumberOfProcessors();
			auto results = new RIORESULT[coreCount];

			while (mIsRunIOWorkThread)
			{
				memset(results, 0, sizeof(results));
				ULONG numResults = RIO.RIODequeueCompletion(mRioCompletionQueues[threadIndex], results, coreCount);

				if (RIO_CORRUPT_CQ == numResults)
				{
					LogFuncPtr((int)LogLevel::fetal, "RIONetManager::IoWorkerThread() - RIO CORRUPT CQ");
					continue;
				}

				for (ULONG i = 0; i < numResults; ++i)
				{
					auto context = reinterpret_cast<RioIoContext*>(results[i].RequestContext);
					auto pSession = mSessionManager.GetSessionObj(context->session_index_);
					auto transferred = results[i].BytesTransferred;
										
					IoWorkerThread_ProceRIORequest(transferred, context, pSession);					
				} 

				ProcessLazyedDisConnectSession(threadIndex);
				
				if (0 == numResults) { Sleep(1); }///< for low cpu-usage
			}

			delete results;
			return;
		}

		void IoWorkerThread_ProceRIORequest(const ULONG transferred, RioIoContext* pContext, Session* pSession)
		{
			auto sessionUniqueIdByRioConext = pContext->SessionUnique;

			if (transferred == 0)
			{
				Disconnect(pSession, sessionUniqueIdByRioConext, DisconnectReason::zero_completion);
			}
			else if (IOType::recv == pContext->io_type_)
			{
				auto pRecvBufferPtr = pSession->RecvCompletion(transferred);
				mNetEventManager->OnReceive(pSession->GetIndex(), (UINT32)transferred, pRecvBufferPtr);

				if (auto result = pSession->PostRecv(); result != NetErrorCode::none)
				{
					if (result == NetErrorCode::recv_rio_fail)
					{
						Disconnect(pSession, sessionUniqueIdByRioConext, DisconnectReason::io_request_recv_error);
					}
				}
			}
			else if (IOType::send == pContext->io_type_)
			{
				pSession->SendCompletion(pContext);

				if (pContext->Length != transferred)
				{
					Disconnect(pSession, sessionUniqueIdByRioConext, DisconnectReason::partial_send_completion);
				}
			}
			else
			{
				char logmsg[256] = { 0, };
				sprintf_s(logmsg, "RIONetManager::IoWorkerThread() - Unknown I/O Type: %d", pContext->io_type_);
				LogFuncPtr((int)LogLevel::error, logmsg);
			}
		}

		void ProcessLazyedDisConnectSession(const int threadIndex)
		{
			while (LazyedDisConnectSessions[threadIndex].empty() == false)
			{
				LazyedDisConnectSession disConnectInfo;
				if (LazyedDisConnectSessions[threadIndex].try_pop(disConnectInfo) == false) { break; }
				
				auto pSession = mSessionManager.GetSessionObj(disConnectInfo.SessionIndex, disConnectInfo.UniqueId);
				if (pSession == nullptr) { break; }

				Disconnect(pSession, disConnectInfo.UniqueId, disConnectInfo.reason);
			}
		}

		void Disconnect(Session* pSession, const UINT64 sessionUniqueIdByContext, const DisconnectReason reason)
		{
			auto curTimeSec = Helper::CurrentSecond();
			auto sessionIndex = pSession->GetIndex();

			if (reason == DisconnectReason::accept_loop_request_queue_error ||
				reason == DisconnectReason::accept_loop_on_connect_rrror)
			{
				pSession->DisConnectByAcceptFail();
				mSessionManager.ReturnSession(sessionIndex, curTimeSec);
				return;
			}

			if (pSession->Disconnect(sessionUniqueIdByContext))
			{
				mNetEventManager->OnClose(sessionIndex, reason);
				mSessionManager.ReturnSession(sessionIndex, curTimeSec);
			}
		}

		
	

	private:
		NetConfig mNetConfig;

		TNetEventManager* mNetEventManager;

		bool mIsRunAcceptThread = false;

		bool mIsRunIOWorkThread = false;
		std::vector<std::thread> mIOWorkThreads;

		std::vector<RIO_CQ> mRioCompletionQueues;
		
		SOCKET	mListenSocket = INVALID_SOCKET;

		SessionManager mSessionManager;

		UINT64 mConnectSequenceId = 0;

		std::vector<concurrency::concurrent_queue<LazyedDisConnectSession>> LazyedDisConnectSessions;
	};

}
