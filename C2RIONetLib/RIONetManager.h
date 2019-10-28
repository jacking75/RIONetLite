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

			if (auto result = BindListenSocket(netConfig.PortNumber); result != NetErrorCode::none)
			{
				return result;
			}
			
			if (RIOAPI::SetFunction(mListenSocket) == false) { return NetErrorCode::rio_function_init_fail; }
			
			if (mSessionManager.PrepareSessionPool(sessionCountPerRioThread, netConfig) == false) { return NetErrorCode::prepare_session_pool_fail; }

			/// listen
			if (SOCKET_ERROR == listen(mListenSocket, SOMAXCONN)) { return NetErrorCode::listen_socket_listen_fail; }

			StartIoThreads(rioThreadCount, netConfig.Conn.MAXSendCountPer1Sec, sessionCountPerRioThread);

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
			mIsRunIOWorkThread = false;

			LogFuncPtr((int)LogLevel::info, "RIONetManager::End - Close mListenSocket");			
			closesocket(mListenSocket);
			
			LogFuncPtr((int)LogLevel::info, "RIONetManager::End - Stop IOThreads");			
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
		NetErrorCode BindListenSocket(UINT16 port)
		{
			/// create TCP socket with RIO mode
			mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_REGISTERED_IO);
			if (mListenSocket == INVALID_SOCKET) { return NetErrorCode::listen_socket_create_fail; }

			int opt = 1;
			setsockopt(mListenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(int));

			/// bind
			SOCKADDR_IN serveraddr;
			ZeroMemory(&serveraddr, sizeof(serveraddr));
			serveraddr.sin_family = AF_INET;
			serveraddr.sin_port = htons(port);
			serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

			if (SOCKET_ERROR == bind(mListenSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr))) { return NetErrorCode::listen_socket_bind_fail; }

			char logmsg[256] = { 0, };
			sprintf_s(logmsg, "RIONetManager::CreateListenSocketAndBind() - Port: %d", port);
			LogFuncPtr((int)LogLevel::info, logmsg);
			return NetErrorCode::none;
		}

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
			char logmsg[256] = { 0, };
			LogFuncPtr((int)LogLevel::info, "RIONetManager::StartAcceptLoop() - Start~~~");

			while (mIsRunIOWorkThread)
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

			LogFuncPtr((int)LogLevel::info, "RIONetManager::StartAcceptLoop() - End!!!");
		}

		bool CreateRioCompletionQueue(const unsigned int maxCQSizePerRIOThread)
		{
			auto rioQueue = RIO.RIOCreateCompletionQueue(maxCQSizePerRIOThread, 0);
			if (rioQueue == RIO_INVALID_CQ) { return false; }
			
			mRioCompletionQueues.push_back(rioQueue);
			return true;
		}

		void StartIoThreads(const UINT32 rioThreadCount, const UINT32 maxSendCountPer1Sec, const UINT32 sessionCountPerRioThread)
		{
			mIsRunIOWorkThread = true;
			const unsigned int maxCQSizePerRIOThread = (1 + maxSendCountPer1Sec) * sessionCountPerRioThread;

			for (UINT32 i = 0; i < rioThreadCount; ++i)
			{
				auto threadIndex = i;
				CreateRioCompletionQueue(maxCQSizePerRIOThread);
				mIOWorkThreads.emplace_back([threadIndex, this]() { IoWorkerThread(threadIndex); });

				// mIOWorkThreads 내부에서 초기화 작업이 있어서 잠시 대기한다
				std::this_thread::sleep_for(std::chrono::milliseconds(32));
			}
		}

		void IoWorkerThread(const int _thread_index)
		{
			auto threadIndex = _thread_index;						
			auto coreCount = Helper::GetNumberOfProcessors();
			auto results = new RIORESULT[coreCount];

			IoWorkerThread_startLog(threadIndex, coreCount);
						
			LazyedDisConnectSessions.emplace_back();

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

			IoWorkerThread_endLog(threadIndex);
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

		
		void IoWorkerThread_startLog(const int _thread_index, const int coreCount)
		{
			char logmsg[256] = { 0, };
			sprintf_s(logmsg, "RIONetManager::IoWorkerThread() - Start. threadIndex: %d, coreCount: %d", _thread_index, coreCount);
			LogFuncPtr((int)LogLevel::info, logmsg);
		}

		void IoWorkerThread_endLog(const int _thread_index)
		{
			char logmsg[256] = { 0, };
			sprintf_s(logmsg, "RIONetManager::IoWorkerThread() - End. threadIndex: %d", _thread_index);
			LogFuncPtr((int)LogLevel::info, logmsg);
		}
	

	private:
		NetConfig mNetConfig;

		TNetEventManager* mNetEventManager;
				
		bool mIsRunIOWorkThread = false;
		std::vector<std::thread> mIOWorkThreads;

		std::vector<RIO_CQ> mRioCompletionQueues;
		
		SOCKET	mListenSocket = INVALID_SOCKET;

		SessionManager mSessionManager;

		UINT64 mConnectSequenceId = 0;

		std::vector<concurrency::concurrent_queue<LazyedDisConnectSession>> LazyedDisConnectSessions;
	};

}
