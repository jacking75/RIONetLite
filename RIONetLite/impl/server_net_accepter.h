#pragma once

#include "../../src/detail/rio_api.h"
#include "../../src/detail/win32_resources.h"
#include "../define.h"
#include "../i_net_event.h"
#include "session_manager.h"
#include "tcp_session.h"

#include <algorithm>
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
      detail::UniqueSocket& listen_socket,
      std::vector<detail::UniqueRioCompletionQueue>& rio_completion_queues,
      SessionManager& session_mgr, INetEventManager* p_net_event_mgr,
      detail::RioApi& rio_api) {
    rio_api_ = &rio_api;
    net_event_mgr_ = p_net_event_mgr;
    listen_socket_ = &listen_socket;
    is_run_.store(true, std::memory_order_release);
    try {
      work_thread_ = std::thread([&listen_socket, &rio_completion_queues,
                                  &session_mgr, p_net_event_mgr, this]() {
        Run(listen_socket, rio_completion_queues, session_mgr, p_net_event_mgr);
      });
    } catch (...) {
      is_run_.store(false, std::memory_order_release);
      listen_socket_ = nullptr;
      return false;
    }
    return true;
  }

  void Run(detail::UniqueSocket& listen_socket,
           std::vector<detail::UniqueRioCompletionQueue>& rio_completion_queues,
           SessionManager& session_mgr, INetEventManager* net_event_mgr) {
    // Stop이 다른 스레드에서 소켓을 닫아 blocked accept를 깨우므로 핸들 값은
    // 루프 진입 시 한 번만 읽는다. 소켓 생성은 이 스레드만 수행한다.
    const SOCKET listen_handle = listen_socket.get();
    std::uint32_t consecutive_failures = 0;

    while (is_run_.load(std::memory_order_acquire)) {
      detail::UniqueSocket accepted_socket(
          accept(listen_handle, nullptr, nullptr));
      if (!accepted_socket) {
        if (!is_run_.load(std::memory_order_acquire)) {
          break;
        }
        net_event_mgr->OnRejectedConnection(
            static_cast<std::uint32_t>(WSAGetLastError()));
        BackOff(consecutive_failures);
        continue;
      }

      auto p_session = session_mgr.AllocClientSession();
      if (p_session == nullptr) {
        accepted_socket.Reset();
        net_event_mgr->OnRejectedConnection(WSAENOBUFS);
        BackOff(consecutive_failures);
        continue;
      }
      consecutive_failures = 0;

      auto thread_index = p_session->GetRioThreadId();
      auto rio_completion_queue = rio_completion_queues[thread_index].get();
      if (NewSession(std::move(accepted_socket), p_session,
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
  // 핸들 고갈이나 세션 풀 고갈처럼 지속되는 실패에서 accept 스레드가 CPU를
  // 소진하고 로그를 폭주시키지 않도록 상한이 있는 지수 백오프를 적용한다.
  void BackOff(std::uint32_t& consecutive_failures) const noexcept {
    ++consecutive_failures;
    Sleep(std::min<DWORD>(kMaxBackOffMs, consecutive_failures));
  }

  bool NewSession(detail::UniqueSocket accepted_socket, TcpSession* p_session,
                  RIO_CQ rio_completion_queue) {
    // 받기와 보내기 모두 세션당 in-flight 요청이 1개다. 보내기는 링에 쌓인
    // 바이트를 하나의 RIO_BUF로 묶어 게시한다.
    if (auto rio_rq = rio_api_->CreateRequestQueue(
            accepted_socket.get(), 1, 1, 1, 1, rio_completion_queue,
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

  static constexpr DWORD kMaxBackOffMs = 100;

  std::atomic<bool> is_run_{false};

  detail::UniqueSocket* listen_socket_ = nullptr;
  detail::RioApi* rio_api_ = nullptr;
  INetEventManager* net_event_mgr_ = nullptr;

  std::thread work_thread_;
};

}  // namespace rionetlite
