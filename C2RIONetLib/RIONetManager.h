#pragma once

#include <vector>
#include <thread>

#include "SessionManager.h"
#include "CommonDef.h"

//TODO 라이브러리에서 락을 잘 걸고 있나?, 또는 불필요한 락을 걸지는 않나?

//TODO 테스트 하기. accept 이후 일부러 sleep을 걸고 이 사이에 클라이언트는 접속 종료, 그리고 PostRecv를 하면 어떤 상황이 발생하나? 비동기 요청 에러 or 비동기 요청 후 결과에서 접속 종료
namespace C2RIONetLib
{	
	template<class TNetEventManager>
	class RIONetManager
	{
	public:
		RIONetManager() = default;
		~RIONetManager() = default;
		
		//TODO 실패 반환이 2개 이상인 것은 반환값 타입을 error 타입으로 바꾸기
		bool Initialize(NetConfig netConfig, TNetEventManager* tNetEventManager)
		{
			if ((netConfig.MaxConnectionCount % netConfig.IoWorkThreadCount) != 0) { return false; }

			auto sessionCountPerRioThread = netConfig.MaxConnectionCount / netConfig.IoWorkThreadCount;
			mNetConfig = netConfig;
			auto rioThreadCount = netConfig.IoWorkThreadCount;
			mNetEventManager = tNetEventManager;

			/// winsock initializing			
			if (WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { return false; }

			/// create TCP socket with RIO mode
			mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_REGISTERED_IO);
			if (mListenSocket == INVALID_SOCKET) { return false; }

			int opt = 1;
			setsockopt(mListenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(int));

			/// bind
			SOCKADDR_IN serveraddr;
			ZeroMemory(&serveraddr, sizeof(serveraddr));
			serveraddr.sin_family = AF_INET;
			serveraddr.sin_port = htons(netConfig.PortNumber);
			serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

			if (SOCKET_ERROR == bind(mListenSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr))) { return false; }

			if (RIOAPI::SetFunction(mListenSocket) == false) { return false; }
			
			if (mSessionManager.PrepareSessionPool(sessionCountPerRioThread, netConfig) == false) { return false; }

			/// listen
			if (SOCKET_ERROR == listen(mListenSocket, SOMAXCONN)) { return false; }

			const unsigned int maxCQSizePerRIOThread = (netConfig.Conn.MAXRecvCountPer1Sec + netConfig.Conn.MAXSendCountPer1Sec) * sessionCountPerRioThread;
			StartIoThreads(rioThreadCount, maxCQSizePerRIOThread);

			LogFuncPtr((int)LogLevel::Info, "RIONetManager::Initialize() - Success!");
			return true;
		}

		void StartAndBlocking()
		{			
			StartAcceptLoop(mNetConfig.Conn);
		}						

		void End()
		{
			LogFuncPtr((int)LogLevel::Info, "RIONetManager::End - Start");

			LogFuncPtr((int)LogLevel::Info, "RIONetManager::End - Close mListenSocket");
			mIsRunAcceptThread = false;
			closesocket(mListenSocket);
			
			LogFuncPtr((int)LogLevel::Info, "RIONetManager::End - Stop IOThreads");
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

			LogFuncPtr((int)LogLevel::Info, "RIONetManager::End - Complete");
		}

		const RIO_CQ& GetCompletionQueue(int threadId) { return mRioCompletionQueues[threadId]; }

		bool Send(const INT32 sessionIndex, const UINT64 uniqueId, const UINT32 dataSize, const char* pData)
		{
			//TODO send 구현하기
		}

	private:		
		void StartAcceptLoop(ConnectionNetConfig netConfig)
		{
			mIsRunAcceptThread = true;

			while (mIsRunAcceptThread)
			{
				SOCKET acceptedSock = accept(mListenSocket, NULL, NULL);
				if (acceptedSock == INVALID_SOCKET)
				{
					LogFuncPtr((int)LogLevel::Error, "RIONetManager::StartAcceptLoop() - accept invalid socket");
					continue;
				}

				/// new client session (should not be under any session locks)
				auto pSession = mSessionManager.IssueClientSession();
				auto clientThreadId = pSession->GetRioThreadId();

				/// connection establishing and then issuing recv
				/// create socket RQ
				/// SEND and RECV within one CQ (you can do with two CQs, seperately)
				auto rioRequestQueue = RIO.RIOCreateRequestQueue(acceptedSock,
					netConfig.MAXRecvCountPer1Sec, 1,
					netConfig.MAXSendCountPer1Sec, 1,
					GetCompletionQueue(clientThreadId),
					GetCompletionQueue(clientThreadId),
					NULL);
				if (rioRequestQueue == RIO_INVALID_RQ)
				{
					char logmsg[256] = { 0, };
					sprintf_s(logmsg, "RIONetManager::StartAcceptLoop() - RIOCreateRequestQueue Error: %d", GetLastError());
					LogFuncPtr((int)LogLevel::Error, logmsg);

					Disconnect(pSession, DisconnectReason::OnConnect_Request_Queue_Error);
					continue;
				}

				++mConnectSequenceId;
				if (pSession->OnConnect(mConnectSequenceId, acceptedSock, rioRequestQueue) == false)
				{
					Disconnect(pSession, DisconnectReason::OnConnect_Error);
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
			/// I/O Thread
			for (decltype(rioThreadCount) i = 0; i < rioThreadCount; ++i)
			{
				CreateRioCompletionQueue(maxCQSizePerRIOThread);
				mIOWorkThreads.emplace_back([&]() { IoWorkerThread(i); });
			}
		}

		void IoWorkerThread(const int _thread_index)
		{
			auto threadId = _thread_index;						

			auto coreCount = Helper::GetNumberOfProcessors();
			auto results = new RIORESULT[coreCount];

			while (mIsRunIOWorkThread)
			{
				memset(results, 0, sizeof(results));
				ULONG numResults = RIO.RIODequeueCompletion(mRioCompletionQueues[threadId], results, coreCount);

				if (0 == numResults)
				{
					Sleep(1); ///< for low cpu-usage
					continue;
				}
				else if (RIO_CORRUPT_CQ == numResults)
				{
					LogFuncPtr((int)LogLevel::Fetal, "RIONetManager::IoWorkerThread() - RIO CORRUPT CQ");
					continue;
				}

				for (ULONG i = 0; i < numResults; ++i)
				{
					auto context = reinterpret_cast<RioIoContext*>(results[i].RequestContext);
					auto pSession = mSessionManager.GetSessionObj(context->session_index_);
					auto transferred = results[i].BytesTransferred;
										
					if (transferred == 0)
					{
						CRASH_ASSERT(pSession->GetRioThreadId() == threadId);

						Disconnect(pSession, DisconnectReason::Zero_Completion);
					}
					else if (IO_RECV == context->io_type_)
					{
						if (false == pSession->PostRecv())
						{
							Disconnect(pSession, DisconnectReason::IO_Request_Recv_Error);
						}
					}
					else if (IO_SEND == context->io_type_)
					{
						pSession->SendCompletion(context);

						if (context->Length != transferred)
						{
							Disconnect(pSession, DisconnectReason::Partial_Send_Completion);
						}						
					}
					else
					{
						char logmsg[256] = { 0, };
						sprintf_s(logmsg, "RIONetManager::IoWorkerThread() - Unknown I/O Type: %d", context->io_type_);
						LogFuncPtr((int)LogLevel::Error, logmsg);
					}					
				} /// for

			} /// while

			delete results;
			return;
		}

		//TODO 이 함수는 꼭 pSession 세션의 IO 스레드에서 호출되어야 한다. 
		// 그래서 강제로 접속 종료, 혹은 Send에 의해서 접속 종료할 때 queue 등을 통해서 해당 IO스레드에 넘긴 후 종료 처리하도록 한다. 이후 send도 모아서 보내게 되면 rio 동작에 lock이 필요 없게 된다.
		void Disconnect(Session* pSession, const DisconnectReason reason)
		{
			if (pSession->Disconnect())
			{
				mNetEventManager->OnClose(pSession->GetIndex(), reason);

				mSessionManager.ReturnSession(pSession->GetIndex());
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
	};

}