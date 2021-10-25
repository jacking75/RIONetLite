#pragma once

#include "server_net_init.h"
#include "server_net_lazyed_dis_conn_manager.h"
#include "server_net_accepter.h"
#include "server_net_io_worker.h"
#include "session_manager.h"
#include "helper.h"
#include "..\i_net_event_manager.h"
#include "..\define.h"

#include <vector>


namespace c2rionet
{	
	class ServerNetManagerImpl
	{
	public:
		ServerNetManagerImpl() = default;
		~ServerNetManagerImpl() = default;
	

		NetErrorCode Initialize(NetConfig net_cfg, INetEventManager* p_net_event_mgr)
		{
			if (auto ret = ServerNetManagerInit::Initialize(listen_socket_, net_cfg); ret != NetErrorCode::kNONE) {
				return ret;
			}
						
			if (session_mgr_.PrepareSessionPool(net_cfg) == false) {
				return NetErrorCode::kPREPARE_SESSION_POOL_FAIL;
			}

			io_worker_.Init(&lazyed_dis_conn_mgr_, &session_mgr_, p_net_event_mgr);

			auto dis_connect_func = [&](TcpSession* p_session, const uint64_t session_uid, const DisconnectReason reason)
			{
				Disconnect(p_session, session_uid, reason);
			};
			io_worker_.DisConnectFunc = dis_connect_func;
			accepter_.DisConnectFunc = dis_connect_func;
			lazyed_dis_conn_mgr_.DisConnectFunc = dis_connect_func;

			net_config_ = net_cfg;
			net_event_mgr_ = p_net_event_mgr;

			LogFuncPtr((int)LogLevel::kINFO, "RIONetManager::Initialize() - Success!");
			return NetErrorCode::kNONE;
		}

		void StartAndBlocking() 
		{ 
			io_worker_.StartIoThreads(net_config_.io_work_thread_count_,
									net_config_.conn_cfg_.max_send_count_per_1sec_,
									net_config_.max_connection_count_,
									net_config_.io_work_thread_count_,
									rio_completion_queues_);
		
			accepter_.Run(net_config_.conn_cfg_, listen_socket_, rio_completion_queues_, session_mgr_, net_event_mgr_);
		}
		
		void End()
		{
			LogFuncPtr((int)LogLevel::kINFO, "RIONetManager::End - Start");
			accepter_.Stop();
			
			LogFuncPtr((int)LogLevel::kINFO, "RIONetManager::End - Close mListenSocket");
			closesocket(listen_socket_);

			LogFuncPtr((int)LogLevel::kINFO, "RIONetManager::End - Stop IOThreads");
			io_worker_.Stop();

			session_mgr_.Dispose();

			WSACleanup();

			LogFuncPtr((int)LogLevel::kINFO, "RIONetManager::End - Complete");
		}
						
		NetErrorCode SendData(const INT32 session_index, const UINT64 session_uid, 
							const UINT32 data_size, const char* p_data)
		{
			if (auto ret = session_mgr_.SendData(session_index, session_uid, data_size, p_data);
						ret != NetErrorCode::kNONE) {
				if (ret == NetErrorCode::kSEND_IO_FAIL) {
					ForcedClose(session_index, DisconnectReason::kSEND_ERROR);
				}
				return ret;
			}
			
			return NetErrorCode::kNONE;
		}

		bool ForcedClose(const INT32 session_index, const UINT64 session_uid)
		{
			if (auto thread_index = session_mgr_.ForcedClose(session_index, session_uid);
				thread_index >= 0) {
				lazyed_dis_conn_mgr_.AddSession(thread_index,
									session_index, session_uid, 
									DisconnectReason::kFORCES_CLOSE_SERVER);
				return true;
			}

			return false;
		}



	private:		
		void Disconnect(TcpSession* p_session, const UINT64 session_uid, const DisconnectReason reason)
		{
			auto curTimeSec = Helper::CurrentSecond();
			auto sessionIndex = p_session->GetIndex();

			if (reason == DisconnectReason::kACCEPT_LOOP_REQUEST_QUEUE_ERROR ||
				reason == DisconnectReason::kkACCEPT_LOOP_ON_CONNECT_ERROR) {
				p_session->DisConnectByAcceptFail();
				session_mgr_.ReturnSession(sessionIndex, curTimeSec);
				return;
			}

			if (p_session->Disconnect(session_uid)) {
				net_event_mgr_->OnClose(sessionIndex, reason);
				session_mgr_.ReturnSession(sessionIndex, curTimeSec);
			}
		}

		void ForcedClose(const int session_index, DisconnectReason reason)
		{
			// 이 함수를 호출할 때는 절대 session_index는 유효한 값이다.
			auto p_session = session_mgr_.GetSessionObj(session_index);
			p_session->SetDisableNetwork();
						
			lazyed_dis_conn_mgr_.AddSession(p_session->GetRioThreadId(), 
									p_session->GetIndex(), p_session->GetUniqueId(), 
									reason);
		}

				

	private:	
		NetConfig net_config_;

		INetEventManager* net_event_mgr_ = nullptr;
				
		ServerNetLazyedDisConnManager lazyed_dis_conn_mgr_;

		Accepter accepter_;

		ServerNetIOWorker io_worker_;
		
		std::vector<RIO_CQ> rio_completion_queues_;
		
		SOCKET	listen_socket_ = INVALID_SOCKET;

		SessionManager session_mgr_;

		UINT64 connect_sequence_id_ = 0;
	};

}
