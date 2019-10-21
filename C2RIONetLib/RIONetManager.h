#pragma once

#include <vector>
#include <thread>

#include "CommonDef.h"
#include "SessionManager.h"


namespace C2RIONetLib
{	
	class RIONetManager
	{
	public:
		RIONetManager() = default;

		~RIONetManager() 
		{
			WSACleanup();
		}

		//TODO 실패 반환이 2개 이상인 것은 반환값 타입을 error 타입으로 바꾸기
		bool Initialize(unsigned short port)
		{
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
			serveraddr.sin_port = htons(port);
			serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

			if (SOCKET_ERROR == bind(mListenSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr))) { return false; }

			if (RIOAPI::SetFunction(mListenSocket) == false) { return false; }
			
			if (mSessionManager.PrepareSessionPool() == false) { return false; }

			LogFuncPtr((int)LogLevel::Info, "RIONetManager::Initialize() - Success!");
			return true;
		}

		bool StartIoThreads()
		{
			/// I/O Thread
			for (int i = 0; i < MAX_RIO_THREAD; ++i)
			{
				m_WorkThreads.emplace_back([&]() { IoWorkerThread(i+1); });
			}
						
			return true;
		}

		bool StartAcceptLoop()
		{
			/// listen
			if (SOCKET_ERROR == listen(mListenSocket, SOMAXCONN))
				return false;


			/// accept loop
			while (true)
			{
				SOCKET acceptedSock = accept(mListenSocket, NULL, NULL);
				if (acceptedSock == INVALID_SOCKET)
				{
					//printf_s("[DEBUG] accept: invalid socket\n");
					continue;
				}

				SOCKADDR_IN clientaddr;
				int addrlen = sizeof(clientaddr);
				getpeername(acceptedSock, (SOCKADDR*)&clientaddr, &addrlen);

				/// new client session (should not be under any session locks)
				ClientSession* client = mSessionManager.IssueClientSession();

				/// connection establishing and then issuing recv
				if (false == client->OnConnect(acceptedSock, &clientaddr))
				{
					client->Disconnect(DR_ONCONNECT_ERROR);
				}
			}

			return true;
		}

		void End()
		{
			//LogFuncPtr((int)LogLevel::Info, "IOCPServer::EndServer - Start");

			//if (m_hWorkIOCP != INVALID_HANDLE_VALUE)
			//{
			//	m_IsRunWorkThread = false;
			//	CloseHandle(m_hWorkIOCP);

			//	for (int i = 0; i < m_WorkThreads.size(); ++i)
			//	{
			//		if (m_WorkThreads[i].get()->joinable())
			//		{
			//			m_WorkThreads[i].get()->join();
			//		}
			//	}
			//}


			//if (m_hLogicIOCP != INVALID_HANDLE_VALUE)
			//{
			//	//PostQueuedCompletionStatus(m_hLogicIOCP, 0, 0, nullptr);
			//	CloseHandle(m_hLogicIOCP);
			//}

			//if (m_ListenSocket != INVALID_SOCKET)
			//{
			//	closesocket(m_ListenSocket);
			//	m_ListenSocket = INVALID_SOCKET;
			//}

			WSACleanup();

			//DestoryConnections();

			//LogFuncPtr((int)LogLevel::Info, "IOCPServer::EndServer - Completion");
		}

		const RIO_CQ& GetCompletionQueue(int threadId) { return mRioCompletionQueue[threadId]; }

	public:
		//static RIO_EXTENSION_FUNCTION_TABLE mRioFunctionTable;

	private:
		void IoWorkerThread(const int _thread_index)
		{
			auto LIoThreadId = _thread_index;


			mRioCompletionQueue[LIoThreadId] = RIO.RIOCreateCompletionQueue(MAX_CQ_SIZE_PER_RIO_THREAD, 0);
			if (mRioCompletionQueue[LIoThreadId] == RIO_INVALID_CQ)
			{
				CRASH_ASSERT(false);
				return;
			}


			RIORESULT results[MAX_RIO_RESULT];

			while (true)
			{
				memset(results, 0, sizeof(results));

				ULONG numResults = RIO.RIODequeueCompletion(mRioCompletionQueue[LIoThreadId], results, MAX_RIO_RESULT);

				if (0 == numResults)
				{
					Sleep(1); ///< for low cpu-usage
					continue;
				}
				else if (RIO_CORRUPT_CQ == numResults)
				{
					printf_s("[DEBUG] RIO CORRUPT CQ \n");
					CRASH_ASSERT(false);
				}

				for (ULONG i = 0; i < numResults; ++i)
				{
					RioIoContext* context = reinterpret_cast<RioIoContext*>(results[i].RequestContext);
					ClientSession* client = mSessionManager.GetSessionObj(context->session_index_);
					ULONG transferred = results[i].BytesTransferred;

					CRASH_ASSERT(context && client);


					if (transferred == 0)
					{
						CRASH_ASSERT(client->GetRioThreadId() == LIoThreadId);

						client->Disconnect(DR_ZERO_COMPLETION);
						//ReleaseContext(context);
						continue;
					}
					else if (IO_RECV == context->io_type_)
					{
						client->RecvCompletion(transferred);

						/// echo back
						if (false == client->PostSend())
						{
							client->Disconnect(DR_IO_REQUEST_SEND_ERROR);
						}

					}
					else if (IO_SEND == context->io_type_)
					{
						client->SendCompletion(transferred);

						if (context->Length != transferred)
						{
							client->Disconnect(DR_PARTIAL_SEND_COMPLETION);
						}
						else if (false == client->PostRecv())
						{
							client->Disconnect(DR_IO_REQUEST_RECV_ERROR);
						}
					}
					else
					{
						printf_s("[DEBUG] Unknown I/O Type: %d\n", context->io_type_);
						CRASH_ASSERT(false);
					}

					//TODO 모두 찾아서 수정하기
					//ReleaseContext(context);

				} /// for


			} /// while


			return;
		}

		//void ReleaseContext(RioIoContext* context)
		//{
		//	//TODO: 구현하기
		//	/// refcount release for i/o context
		//	//context->mClientSession->ReleaseRef();

		//	/// context release
		//	delete context;
		//}

	private:
		std::vector<std::thread> m_WorkThreads;

		/*static*/ RIO_CQ mRioCompletionQueue[MAX_RIO_THREAD + 1];

		SOCKET	mListenSocket = INVALID_SOCKET;

		SessionManager mSessionManager;

	};
		

	//extern RIONetManager* GRioManager;

	//#define RIO	RIONetManager::mRioFunctionTable

}