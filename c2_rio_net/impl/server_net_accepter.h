#pragma once

#include "tcp_session.h"
#include "session_manager.h"
#include "helper.h"
#include "../i_net_event_manager.h"
#include "../define.h"

#include <functional>
#include <cstdint>

namespace c2rionet
{
	class Accepter
	{
	public:
		Accepter() = default;
		~Accepter() = default;

		void Run(ConnectionNetConfig net_cfg, 
				SOCKET listen_socket, 
				std::vector<RIO_CQ>& rio_completion_queues,
				SessionManager& session_mgr, 
				INetEventManager* net_event_mgr)
		{
			char logmsg[256] = { 0, };
			LogFuncPtr((int)LogLevel::kINFO, "RIONetManager::StartAcceptLoop() - Start~~~");

			is_run_ = true;
			while (is_run_) {
				SOCKET acceptedSock = accept(listen_socket, NULL, NULL);
				if (acceptedSock == INVALID_SOCKET) {
					LogFuncPtr((int)LogLevel::kERROR, "RIONetManager::StartAcceptLoop() - accept invalid socket");
					continue;
				}

				auto curTimeSec = Helper::CurrentSecond();

				/// new client session (should not be under any session locks)
				auto pSession = session_mgr.IssueClientSession(curTimeSec);
				if (pSession == nullptr) {
					//사용할 수 있는 세션이 없는 상태이므로 연결을 끊고, 잠시 대기한다
					LogFuncPtr((int)LogLevel::kDEBUG, "Accepter::Run() - Empry Session Pool");
					closesocket(acceptedSock);
					Sleep(1000);
					continue;
				}

				auto clientThreadId = pSession->GetRioThreadId();

				/// connection establishing and then issuing recv
				/// create socket RQ
				/// SEND and RECV within one CQ (you can do with two CQs, seperately)
				if (auto rioRequestQueue = RIO.RIOCreateRequestQueue(acceptedSock,
													1, 1, // 받기는 연속으로 요청하지 않으므로 1
													net_cfg.max_send_count_per_1sec_, 1,
													rio_completion_queues[clientThreadId],
													rio_completion_queues[clientThreadId],
													NULL); rioRequestQueue != RIO_INVALID_RQ) {
					
					++connect_sequence_id_;
					if (pSession->OnConnect(connect_sequence_id_, acceptedSock, rioRequestQueue) == false) {
						DisConnectFunc(pSession, 0, DisconnectReason::kkACCEPT_LOOP_ON_CONNECT_ERROR);
						continue;
					}
				} else {
					sprintf_s(logmsg, "Accepter::Run() - RIOCreateRequestQueue Error: %d", GetLastError());
					LogFuncPtr((int)LogLevel::kERROR, logmsg);

					DisConnectFunc(pSession, 0, DisconnectReason::kACCEPT_LOOP_REQUEST_QUEUE_ERROR);
					continue;
				}

				net_event_mgr->OnConnect(pSession->GetIndex(), pSession->GetUniqueId());
			}

			LogFuncPtr((int)LogLevel::kINFO, "RIONetManager::StartAcceptLoop() - End!!!");
		}


		std::function<void(TcpSession*, const uint64_t, const DisconnectReason)> DisConnectFunc;


		void Stop() 
		{
			is_run_ = false;
		}


	private:
		bool is_run_;
		uint64_t connect_sequence_id_;
	};
}