#pragma once

#include "../../src/detail/win32_resources.h"
#include "../define.h"
#include "../i_net_event.h"
#include "helper.h"
#include "send_manager.h"
#include "session_manager.h"
#include "tcp_session.h"

#include <atomic>
#include <cstdint>
#include <format>
#include <functional>
#include <utility>

namespace rionetlite {
class Accepter {
 public:
  Accepter() = default;
  ~Accepter() = default;

  std::function<void(TcpSession*, const uint64_t, const DisconnectReason)>
      DisConnectFunc;

  void Start(
      SessionNetConfig net_cfg, detail::UniqueSocket& listen_socket,
      std::vector<detail::UniqueRioCompletionQueue>& rio_completion_queues,
      SessionManager& session_mgr, INetEventManager* p_net_event_mgr,
      [[maybe_unused]] SendManager* p_send_mgr) {
    listen_socket_ = &listen_socket;
    is_run_.store(true, std::memory_order_release);
    work_thread_ = std::thread([&, p_net_event_mgr, p_send_mgr]() {
      Run(net_cfg, listen_socket, rio_completion_queues, session_mgr,
          p_net_event_mgr, p_send_mgr);
    });
  }

  void Run(SessionNetConfig net_cfg, detail::UniqueSocket& listen_socket,
           std::vector<detail::UniqueRioCompletionQueue>& rio_completion_queues,
           SessionManager& session_mgr, INetEventManager* net_event_mgr,
           [[maybe_unused]] SendManager* p_send_mgr) {
    LogFuncPtr((int)LogLevel::kINFO, "Accepter::Run() - Start~~~");

    while (is_run_.load(std::memory_order_acquire)) {
      WSAPOLLFD poll_descriptor{
          .fd = listen_socket.get(),
          .events = POLLRDNORM,
          .revents = 0,
      };
      const int poll_result = WSAPoll(&poll_descriptor, 1, 100);
      if (!is_run_.load(std::memory_order_acquire)) {
        break;
      }
      if (poll_result == 0) {
        continue;
      }
      if (poll_result == SOCKET_ERROR ||
          (poll_descriptor.revents & POLLRDNORM) == 0) {
        LogFuncPtr((int)LogLevel::kERROR,
                   "Accepter::Run() - listen socket poll failed");
        continue;
      }

      detail::UniqueSocket accepted_socket(
          accept(listen_socket.get(), nullptr, nullptr));
      if (!accepted_socket) {
        if (!is_run_.load(std::memory_order_acquire)) {
          break;
        }
        LogFuncPtr((int)LogLevel::kERROR,
                   "Accepter::Run() - accept invalid socket");
        continue;
      }

      auto cur_time_sec = Helper::CurrentSecond();

      auto p_session = session_mgr.AllocClientSession(cur_time_sec);
      if (p_session == nullptr) {
        // 사용할 수 있는 세션이 없는 상태이므로 연결을 끊고, 잠시 대기한다
        LogFuncPtr((int)LogLevel::kDEBUG,
                   "Accepter::Run() - Empry Session Pool");
        accepted_socket.Reset();
        Sleep(1000);
        continue;
      }

      auto thread_index = p_session->GetRioThreadId();
      auto rio_completion_queue = rio_completion_queues[thread_index].get();
      if (NewSession(std::move(accepted_socket), p_session,
                     net_cfg.max_send_count_per_1sec_,
                     rio_completion_queue) == false) {
        continue;
      }

#ifdef __USE_SEND_THREAD__
      p_send_mgr->OnConnectSession(p_session->GetIndex(), p_session->GetUID(),
                                   p_session->GetRIORQ());
#endif

      net_event_mgr->OnConnect(p_session->GetIndex(), p_session->GetUID());
    }

    LogFuncPtr((int)LogLevel::kINFO, "Accepter::Run() - End!!!");
  }

  void Stop() {
    LogFuncPtr((int)LogLevel::kINFO, "Accepter::Stop - Start");

    is_run_.store(false, std::memory_order_release);

    if (listen_socket_ != nullptr) {
      listen_socket_->Reset();
    }

    if (work_thread_.joinable()) {
      work_thread_.join();
    }

    LogFuncPtr((int)LogLevel::kINFO, "Accepter::Stop - End");
  }

 private:
  bool NewSession(detail::UniqueSocket accepted_socket, TcpSession* p_session,
                  int32_t max_send_count_per_1sec,
                  RIO_CQ rio_completion_queue) {
    if (auto rio_rq = RIO.RIOCreateRequestQueue(
            accepted_socket.get(), 1, 1,  // 받기는 연속으로 요청하지 않으므로 1
            max_send_count_per_1sec, 1, rio_completion_queue,
            rio_completion_queue, NULL);
        rio_rq != RIO_INVALID_RQ) {
      ++connect_sequence_id_;
      if (p_session->OnConnect(connect_sequence_id_, std::move(accepted_socket),
                               rio_rq) == false) {
        DisConnectFunc(p_session, 0,
                       DisconnectReason::kACCEPT_LOOP_FAIL_ON_CONNECT);
        return false;
      }
    } else {
      FailCreateRIOQueue(p_session);
      return false;
    }

    return true;
  }

  void FailCreateRIOQueue(TcpSession* p_session) {
    LogFuncPtr((int)LogLevel::kERROR,
               std::format("Accepter::Run() - RIOCreateRequestQueue Error: {0}",
                           GetLastError())
                   .c_str());

    DisConnectFunc(p_session, 0,
                   DisconnectReason::kACCEPT_LOOP_REQUEST_QUEUE_ERROR);
  }

  std::atomic<bool> is_run_{false};

  uint64_t connect_sequence_id_ = 0;

  detail::UniqueSocket* listen_socket_ = nullptr;

  std::thread work_thread_;
};

}  // namespace rionetlite
