#pragma once

#include <vector>
#include <thread>
#include <concurrent_queue.h>

#include "session_manager.h"
#include "..\define.h"
#include "helper.h"


namespace c2rionet
{	
	template<class TNetEventManager>
	class ServerNetManagerImpl
	{
	public:
		ServerNetManagerImpl() = default;
		~ServerNetManagerImpl() = default;
	

		NetErrorCode Initialize(NetConfig net_cfg, TNetEventManager* p_net_event_mgr)
		{
			if ((net_cfg.max_connection_count_ % net_cfg.io_work_thread_count_) != 0) { return NetErrorCode::kSESSION_COUNT_PER_THREAD_INCORRECT; }

			auto sessionCountPerRioThread = net_cfg.max_connection_count_ / net_cfg.io_work_thread_count_;
			net_config_ = net_cfg;
			auto rioThreadCount = net_cfg.io_work_thread_count_;
			net_event_mgr_ = p_net_event_mgr;

			/// winsock initializing			
			if (WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { return NetErrorCode::kWSASTARTUP_FAIL; }

			if (auto result = BindListenSocket(net_cfg.port_number_); result != NetErrorCode::kNONE)
			{
				return result;
			}

			if (RIOAPI::SetFunction(listen_socket_) == false) { return NetErrorCode::kRIO_FUNCTION_INIT_FAIL; }

			if (session_mgr_.PrepareSessionPool(sessionCountPerRioThread, net_cfg) == false) { return NetErrorCode::kPREPARE_SESSION_POOL_FAIL; }

			/// listen
			if (SOCKET_ERROR == listen(listen_socket_, SOMAXCONN)) { return NetErrorCode::kLISTEN_SOCKET_LISTEN_FAIL; }

			StartIoThreads(rioThreadCount, net_cfg.conn_cfg_.max_send_count_per_1sec_, sessionCountPerRioThread);

			LogFuncPtr((int)LogLevel::kINFO, "RIONetManager::Initialize() - Success!");
			return NetErrorCode::kNONE;
		}

		void StartAndBlocking() { 
			StartAcceptLoop(net_config_.conn_cfg_); 
		}
		
		void End()
		{
			LogFuncPtr((int)LogLevel::kINFO, "RIONetManager::End - Start");
			is_run_io_work_thread_ = false;

			LogFuncPtr((int)LogLevel::kINFO, "RIONetManager::End - Close mListenSocket");
			closesocket(listen_socket_);

			LogFuncPtr((int)LogLevel::kINFO, "RIONetManager::End - Stop IOThreads");
			for (auto& thread : io_work_threads_)
			{
				if (thread.joinable())
				{
					thread.join();
				}
			}


			session_mgr_.Dispose();

			WSACleanup();

			LogFuncPtr((int)LogLevel::kINFO, "RIONetManager::End - Complete");
		}
		
		const RIO_CQ& GetCompletionQueue(int thread_id) { 
			return rio_completion_queues_[thread_id]; 
		}
						
		NetErrorCode BindListenSocket(UINT16 port)
		{
			/// create TCP socket with RIO mode
			listen_socket_ = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_REGISTERED_IO);
			if (listen_socket_ == INVALID_SOCKET) { return NetErrorCode::kLISTEN_SOCKET_CREATE_FAIL; }

			int opt = 1;
			setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(int));

			/// bind
			SOCKADDR_IN serveraddr;
			ZeroMemory(&serveraddr, sizeof(serveraddr));
			serveraddr.sin_family = AF_INET;
			serveraddr.sin_port = htons(port);
			serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

			if (SOCKET_ERROR == bind(listen_socket_, (SOCKADDR*)&serveraddr, sizeof(serveraddr))) { return NetErrorCode::kLISTEN_SOCKET_BIND_FAIL; }

			char logmsg[256] = { 0, };
			sprintf_s(logmsg, "RIONetManager::CreateListenSocketAndBind() - Port: %d", port);
			LogFuncPtr((int)LogLevel::kINFO, logmsg);
			return NetErrorCode::kNONE;
		}

		NetErrorCode SendData(const INT32 session_index, const UINT64 session_uid, const UINT32 data_size, const char* p_data)
		{
			auto session = session_mgr_.GetSessionObj(session_index, session_uid);
			if (session == nullptr)
			{
				return NetErrorCode::kSEND_INVALID_SESSION;
			}

			if (session->IsEnableNetwork() == false)
			{
				return NetErrorCode::kSEND_SESSION_NETWORK_DISABLE;
			}

			if (session->PostSend(data_size, p_data) == false)
			{
				ForcedClose(session, DisconnectReason::kSEND_ERROR);
				return NetErrorCode::kSEND_IO_FAIL;
			}

			return NetErrorCode::kNONE;
		}

		void ForcedClose(const INT32 session_index, const UINT64 session_uid)
		{
			auto session = session_mgr_.GetSessionObj(session_index, session_uid);
			if (session == nullptr)
			{
				return;
			}

			session->SetDisableNetwork();

			auto threadIndex = session->GetRioThreadId();
			lazyed_dis_connect_sessions_[threadIndex].push({ session_index, session_uid, DisconnectReason::kFORCES_CLOSE_SERVER });
		}

		void ForcedClose(TcpSession* p_session, DisconnectReason reason)
		{
			p_session->SetDisableNetwork();

			auto threadIndex = p_session->GetRioThreadId();
			auto sessionIndex = p_session->GetIndex();
			auto sessionUniqueId = p_session->GetUniqueId();

			lazyed_dis_connect_sessions_[threadIndex].push({ sessionIndex, sessionUniqueId, reason });
		}

		void StartAcceptLoop(ConnectionNetConfig net_cfg)
		{
			char logmsg[256] = { 0, };
			LogFuncPtr((int)LogLevel::kINFO, "RIONetManager::StartAcceptLoop() - Start~~~");

			while (is_run_io_work_thread_)
			{
				SOCKET acceptedSock = accept(listen_socket_, NULL, NULL);
				if (acceptedSock == INVALID_SOCKET)
				{
					LogFuncPtr((int)LogLevel::kERROR, "RIONetManager::StartAcceptLoop() - accept invalid socket");
					continue;
				}

				auto curTimeSec = Helper::CurrentSecond();

				/// new client session (should not be under any session locks)
				auto pSession = session_mgr_.IssueClientSession(curTimeSec);
				if (pSession == nullptr)
				{			
					//사용할 수 있는 세션이 없는 상태이므로 연결을 끊고, 잠시 대기한다
					LogFuncPtr((int)LogLevel::kDEBUG, "RIONetManager::StartAcceptLoop() - Empry Session Pool");
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
					net_cfg.max_send_count_per_1sec_, 1,
					GetCompletionQueue(clientThreadId),
					GetCompletionQueue(clientThreadId),
					NULL);
				if (rioRequestQueue == RIO_INVALID_RQ)
				{
					sprintf_s(logmsg, "RIONetManager::StartAcceptLoop() - RIOCreateRequestQueue Error: %d", GetLastError());
					LogFuncPtr((int)LogLevel::kERROR, logmsg);

					Disconnect(pSession, 0, DisconnectReason::kACCEPT_LOOP_REQUEST_QUEUE_ERROR);
					continue;
				}

				++connect_sequence_id_;
				if (pSession->OnConnect(connect_sequence_id_, acceptedSock, rioRequestQueue) == false)
				{
					Disconnect(pSession, 0, DisconnectReason::kkACCEPT_LOOP_ON_CONNECT_ERROR);
					continue;
				}

				net_event_mgr_->OnConnect(pSession->GetIndex(), pSession->GetUniqueId());
			}

			LogFuncPtr((int)LogLevel::kINFO, "RIONetManager::StartAcceptLoop() - End!!!");
		}

		bool CreateRioCompletionQueue(const unsigned int max_cq_size_per_rio_thread)
		{
			auto rioQueue = RIO.RIOCreateCompletionQueue(max_cq_size_per_rio_thread, 0);
			if (rioQueue == RIO_INVALID_CQ) { return false; }
			
			rio_completion_queues_.push_back(rioQueue);
			return true;
		}

		void StartIoThreads(const UINT32 rio_thread_count, const UINT32 max_send_count_per_1sec, const UINT32 session_count_per_rio_thread)
		{
			is_run_io_work_thread_ = true;
			const unsigned int maxCQSizePerRIOThread = (1 + max_send_count_per_1sec) * session_count_per_rio_thread;

			for (UINT32 i = 0; i < rio_thread_count; ++i)
			{
				auto threadIndex = i;
				CreateRioCompletionQueue(maxCQSizePerRIOThread);
				io_work_threads_.emplace_back([threadIndex, this]() { IoWorkerThread(threadIndex); });

				// mIOWorkThreads 내부에서 초기화 작업이 있어서 잠시 대기한다
				std::this_thread::sleep_for(std::chrono::milliseconds(32));
			}
		}

		void IoWorkerThread(const int thread_index)
		{
			auto threadIndex = thread_index;						
			auto coreCount = Helper::GetNumberOfProcessors();
			auto results = new RIORESULT[coreCount];

			IoWorkerThread_startLog(threadIndex, coreCount);
						
			lazyed_dis_connect_sessions_.emplace_back();

			while (is_run_io_work_thread_)
			{
				memset(results, 0, sizeof(results));
				ULONG numResults = RIO.RIODequeueCompletion(rio_completion_queues_[threadIndex], results, coreCount);

				if (RIO_CORRUPT_CQ == numResults)
				{
					LogFuncPtr((int)LogLevel::kFATAL, "RIONetManager::IoWorkerThread() - RIO CORRUPT CQ");
					continue;
				}

				for (ULONG i = 0; i < numResults; ++i)
				{
					auto context = reinterpret_cast<RioIoContext*>(results[i].RequestContext);
					auto pSession = session_mgr_.GetSessionObj(context->session_index_);
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

		void IoWorkerThread_ProceRIORequest(const ULONG transferred, RioIoContext* p_context, TcpSession* p_session)
		{
			auto sessionUniqueIdByRioConext = p_context->session_unique_Id_;

			if (transferred == 0)
			{
				Disconnect(p_session, sessionUniqueIdByRioConext, DisconnectReason::kZERO_COMPLETION);
			}
			else if (IOType::kRECV == p_context->io_type_)
			{
				RecvCompletion(p_session, transferred);
				
				if (auto result = p_session->PostRecv(); result != NetErrorCode::kNONE)
				{
					if (result == NetErrorCode::kRECV_RIO_FAIL)
					{
						Disconnect(p_session, sessionUniqueIdByRioConext, DisconnectReason::kIO_REQUEST_RECV_ERROR);
					}
				}
			}
			else if (IOType::kSEND == p_context->io_type_)
			{
				auto postDataLen = p_context->Length;
				p_session->SendCompletion(p_context);

				if (postDataLen != transferred)
				{
					Disconnect(p_session, sessionUniqueIdByRioConext, DisconnectReason::kPARITAL_SEND_COMPLETION);
				}
			}
			else
			{
				char logmsg[256] = { 0, };
				sprintf_s(logmsg, "RIONetManager::IoWorkerThread() - Unknown I/O Type: %d", p_context->io_type_);
				LogFuncPtr((int)LogLevel::kERROR, logmsg);
			}
		}

		// 받기 버퍼의 읽은 위치도 여기서 이동 시키므로 링버퍼의 데이터가 겹치는 것을 막고 싶으면 애플리케이션에서 OnReceive의 데이터를 다 사용할 때까지 잘 보관해야 한다.
		void RecvCompletion(TcpSession* p_session, const UINT32 transferred)
		{
			p_session->RecvCompletion(transferred);

			auto [readSize, pReadData] = p_session->ReadRecvBuffer();
			auto usedSize = net_event_mgr_->OnReceive(p_session->GetIndex(),
				p_session->GetUniqueId(),
				readSize,
				pReadData);
			p_session->IncremantReadPosRecvBuffer(usedSize);
		}
								
		void ProcessLazyedDisConnectSession(const int thread_index)
		{
			while (lazyed_dis_connect_sessions_[thread_index].empty() == false)
			{
				LazyedDisConnectSession disConnectInfo;
				if (lazyed_dis_connect_sessions_[thread_index].try_pop(disConnectInfo) == false) { break; }
				
				auto pSession = session_mgr_.GetSessionObj(disConnectInfo.session_index_, disConnectInfo.unique_id_);
				if (pSession == nullptr) { break; }

				Disconnect(pSession, disConnectInfo.unique_id_, disConnectInfo.reason_);
			}
		}

		void Disconnect(TcpSession* p_session, const UINT64 session_uid_by_context, const DisconnectReason reason)
		{
			auto curTimeSec = Helper::CurrentSecond();
			auto sessionIndex = p_session->GetIndex();

			if (reason == DisconnectReason::kACCEPT_LOOP_REQUEST_QUEUE_ERROR ||
				reason == DisconnectReason::kkACCEPT_LOOP_ON_CONNECT_ERROR)
			{
				p_session->DisConnectByAcceptFail();
				session_mgr_.ReturnSession(sessionIndex, curTimeSec);
				return;
			}

			if (p_session->Disconnect(session_uid_by_context))
			{
				net_event_mgr_->OnClose(sessionIndex, reason);
				session_mgr_.ReturnSession(sessionIndex, curTimeSec);
			}
		}

		
		
		void IoWorkerThread_startLog(const int thread_index, const int core_count)
		{
			char logmsg[256] = { 0, };
			sprintf_s(logmsg, "RIONetManager::IoWorkerThread() - Start. threadIndex: %d, coreCount: %d", thread_index, core_count);
			LogFuncPtr((int)LogLevel::kINFO, logmsg);
		}

		void IoWorkerThread_endLog(const int threadIndex_)
		{
			char logmsg[256] = { 0, };
			sprintf_s(logmsg, "RIONetManager::IoWorkerThread() - End. threadIndex: %d", threadIndex_);
			LogFuncPtr((int)LogLevel::kINFO, logmsg);
		}
	



	private:	
		NetConfig net_config_;

		TNetEventManager* net_event_mgr_;
				
		bool is_run_io_work_thread_ = false;
		std::vector<std::thread> io_work_threads_;

		std::vector<RIO_CQ> rio_completion_queues_;
		
		SOCKET	listen_socket_ = INVALID_SOCKET;

		SessionManager session_mgr_;

		UINT64 connect_sequence_id_ = 0;

		std::vector<concurrency::concurrent_queue<LazyedDisConnectSession>> lazyed_dis_connect_sessions_;	
	};

}
