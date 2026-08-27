#pragma once

#include "../../src/detail/rio_api.h"
#include "../../src/detail/win32_resources.h"
#include "../define.h"
#include "../i_net_event.h"
#include "session_manager.h"
#include "tcp_session.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>
#include <utility>
#include <vector>

namespace rionetlite {
class Accepter {
 public:
  Accepter() = default;
  ~Accepter() = default;

  std::function<bool(TcpSession*, const uint64_t, const DisconnectReason)>
      DisConnectFunc;

  bool Start(
      SessionNetConfig net_cfg, detail::UniqueSocket& listen_socket,
      std::vector<detail::UniqueRioCompletionQueue>& rio_completion_queues,
      SessionManager& session_mgr, INetEventManager* p_net_event_mgr,
      detail::RioApi& rio_api) {
    rio_api_ = &rio_api;
    net_event_mgr_ = p_net_event_mgr;
    listen_socket_ = &listen_socket;
    is_run_.store(true, std::memory_order_release);
    try {
      work_thread_ =
          std::thread([net_cfg, &listen_socket, &rio_completion_queues,
                       &session_mgr, p_net_event_mgr, this]() {
            Run(net_cfg, listen_socket, rio_completion_queues, session_mgr,
                p_net_event_mgr);
          });
    } catch (...) {
      is_run_.store(false, std::memory_order_release);
      listen_socket_ = nullptr;
      return false;
    }
    return true;
  }

  void Run(SessionNetConfig net_cfg, detail::UniqueSocket& listen_socket,
           std::vector<detail::UniqueRioCompletionQueue>& rio_completion_queues,
           SessionManager& session_mgr, INetEventManager* net_event_mgr) {
    while (is_run_.load(std::memory_order_acquire)) {
      detail::UniqueSocket accepted_socket(
          accept(listen_socket.get(), nullptr, nullptr));
      if (!accepted_socket) {
        if (!is_run_.load(std::memory_order_acquire)) {
          break;
        }
        net_event_mgr->OnRejectedConnection(
            static_cast<std::uint32_t>(WSAGetLastError()));
        continue;
      }

      auto p_session = session_mgr.AllocClientSession();
      if (p_session == nullptr) {
        accepted_socket.Reset();
        net_event_mgr->OnRejectedConnection(WSAENOBUFS);
        continue;
      }

      auto thread_index = p_session->GetRioThreadId();
      auto rio_completion_queue = rio_completion_queues[thread_index].get();
      if (NewSession(std::move(accepted_socket), p_session,
                     net_cfg.max_send_count_per_1sec_,
                     rio_completion_queue) == false) {
        continue;
      }

      net_event_mgr->OnConnect(p_session->GetIndex(), p_session->GetUID());
      if (p_session->PostRecv() != NetErrorCode::kNONE) {
        net_event_mgr->OnTransportError(
            p_session->GetRioThreadId(), p_session->GetIndex(),
            p_session->GetUID(), "initial_receive",
            static_cast<std::uint32_t>(WSAGetLastError()));
        static_cast<void>(
            DisConnectFunc(p_session, p_session->GetUID(),
                           DisconnectReason::kIO_REQUEST_RECV_ERROR));
      }
    }
  }

  void Stop() {
    is_run_.store(false, std::memory_order_release);

    if (listen_socket_ != nullptr) {
      listen_socket_->Reset();
    }

    if (work_thread_.joinable()) {
      work_thread_.join();
    }
  }

 private:
  bool NewSession(detail::UniqueSocket accepted_socket, TcpSession* p_session,
                  int32_t max_send_count_per_1sec,
                  RIO_CQ rio_completion_queue) {
    if (auto rio_rq = rio_api_->CreateRequestQueue(
            accepted_socket.get(), 1, 1,  // 받기는 연속으로 요청하지 않으므로 1
            max_send_count_per_1sec, 1, rio_completion_queue,
            rio_completion_queue, nullptr);
        rio_rq != RIO_INVALID_RQ) {
      if (p_session->OnConnect(p_session->GetUID(), std::move(accepted_socket),
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
    if (net_event_mgr_ != nullptr) {
      net_event_mgr_->OnTransportError(
          p_session->GetRioThreadId(), p_session->GetIndex(),
          p_session->GetUID(), "create_request_queue",
          static_cast<std::uint32_t>(GetLastError()));
    }

    DisConnectFunc(p_session, 0,
                   DisconnectReason::kACCEPT_LOOP_REQUEST_QUEUE_ERROR);
  }

  std::atomic<bool> is_run_{false};

  detail::UniqueSocket* listen_socket_ = nullptr;
  detail::RioApi* rio_api_ = nullptr;
  INetEventManager* net_event_mgr_ = nullptr;

  std::thread work_thread_;
};

}  // namespace rionetlite
