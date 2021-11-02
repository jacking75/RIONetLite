#pragma once

#include "tcp_session.h"
#include "session_manager.h"
#include "send_manager.h"
#include "helper.h"
#include "../i_net_event.h"
#include "../define.h"

#include <functional>
#include <format>
#include <cstdint>

namespace c2rionet
{
	class Accepter
	{
	public:
		Accepter() = default;
		~Accepter() = default;

		std::function<void(TcpSession*, const uint64_t, const DisconnectReason)> DisConnectFunc;

		void Start(SessionNetConfig net_cfg,
					SOCKET listen_socket,
					std::vector<RIO_CQ>& rio_completion_queues,
					SessionManager& session_mgr,
					INetEventManager* net_event_mgr,
					[[maybe_unused]] SendManager* p_send_mgr)
		{
			work_thread_ = std::thread([&]() {
					Run(net_cfg,
						listen_socket,
						rio_completion_queues,
						session_mgr,
						net_event_mgr,
						p_send_mgr);
				});
		}

		void Run(SessionNetConfig net_cfg, 
				SOCKET listen_socket, 
				std::vector<RIO_CQ>& rio_completion_queues,
				SessionManager& session_mgr, 
				INetEventManager* net_event_mgr, 
			[[maybe_unused]] SendManager* p_send_mgr)
		{
			LogFuncPtr((int)LogLevel::kINFO, "RIONetManager::StartAcceptLoop() - Start~~~");

			listen_socket_ = listen_socket;
			is_run_ = true;
			while (is_run_) {
				SOCKET accepted_sock = accept(listen_socket, NULL, NULL);
				if (accepted_sock == INVALID_SOCKET) {
					LogFuncPtr((int)LogLevel::kERROR, "RIONetManager::StartAcceptLoop() - accept invalid socket");
					continue;
				}

				auto cur_time_sec = Helper::CurrentSecond();

				auto p_session = session_mgr.AllocClientSession(cur_time_sec);
				if (p_session == nullptr) {
					//사용할 수 있는 세션이 없는 상태이므로 연결을 끊고, 잠시 대기한다
					LogFuncPtr((int)LogLevel::kDEBUG, "Accepter::Run() - Empry Session Pool");
					closesocket(accepted_sock);
					Sleep(1000);
					continue;
				}

				auto thread_index = p_session->GetRioThreadId();
				auto rio_completion_queue = rio_completion_queues[thread_index];
				if (NewSession(accepted_sock,
								p_session,
								net_cfg.max_send_count_per_1sec_,
								rio_completion_queue) == false) {
					continue;
				}

#ifdef __USE_SEND_THREAD__
				p_send_mgr->OnConnectSession(p_session->GetIndex(), p_session->GetUID(), p_session->GetRIORQ());
#endif

				net_event_mgr->OnConnect(p_session->GetIndex(), p_session->GetUID());
			}

			LogFuncPtr((int)LogLevel::kINFO, "RIONetManager::StartAcceptLoop() - End!!!");
		}

		void Stop() 
		{
			LogFuncPtr((int)LogLevel::kINFO, "Accepter::Stop - Start");

			is_run_ = false;

			closesocket(listen_socket_);

			if (work_thread_.joinable()) {
				work_thread_.join();
			}

			LogFuncPtr((int)LogLevel::kINFO, "Accepter::Stop - End");
		}


	private:
		bool NewSession(SOCKET accepted_sock, 
						TcpSession* p_session, 
						int32_t max_send_count_per_1sec,
						RIO_CQ rio_completion_queue)
		{
			if (auto rio_rq = RIO.RIOCreateRequestQueue(accepted_sock,
												1, 1, // 받기는 연속으로 요청하지 않으므로 1
												max_send_count_per_1sec, 1,
												rio_completion_queue,
												rio_completion_queue,
												NULL);
				rio_rq != RIO_INVALID_RQ) {

				++connect_sequence_id_;
				if (p_session->OnConnect(connect_sequence_id_,
					accepted_sock,
					rio_rq) == false) {
					DisConnectFunc(p_session, 0, DisconnectReason::kACCEPT_LOOP_FAIL_ON_CONNECT);
					return false;
				}
			} else {
				FailCreateRIOQueue(p_session);
				return false;
			}

			return true;
		}

		void FailCreateRIOQueue(TcpSession* p_session)
		{
			LogFuncPtr((int)LogLevel::kERROR,
				std::format("Accepter::Run() - RIOCreateRequestQueue Error: {0}", GetLastError()).c_str());

			DisConnectFunc(p_session, 0, DisconnectReason::kACCEPT_LOOP_REQUEST_QUEUE_ERROR);
		}


		bool is_run_;

		uint64_t connect_sequence_id_;

		SOCKET listen_socket_;

		std::thread work_thread_;
	};
}