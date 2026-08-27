#pragma once

#include "../../src/detail/rio_api.h"
#include "../../src/detail/win32_resources.h"
#include "..\define.h"
#include "..\i_net_event.h"
#include "server_net_accepter.h"
#include "server_net_init.h"
#include "server_net_io_worker.h"
#include "server_net_lazyed_dis_conn_manager.h"
#include "session_manager.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

namespace rionetlite {
class ServerNetManagerImpl {
 public:
  ServerNetManagerImpl() = default;
  ~ServerNetManagerImpl() { static_cast<void>(End(false, 5000)); }

  NetErrorCode Initialize(NetConfig net_cfg,
                          INetEventManager* p_net_event_mgr) {
    if (initialized_) {
      return NetErrorCode::kINVALID_CONFIG;
    }
    if (p_net_event_mgr == nullptr) {
      return NetErrorCode::kINVALID_CONFIG;
    }

    if (auto ret = ServerNetManagerInit::Initialize(
            winsock_runtime_, listen_socket_, rio_api_, socket_api_, net_cfg);
        ret != NetErrorCode::kNONE) {
      return ret;
    }

    if (session_mgr_.PrepareSessionPool(net_cfg, rio_api_) == false) {
      session_mgr_.Dispose();
      return NetErrorCode::kPREPARE_SESSION_POOL_FAIL;
    }

    io_worker_.Init(&lazyed_dis_conn_mgr_, &session_mgr_, p_net_event_mgr,
                    rio_api_);

    SetDelegateDisConnect();

    net_config_ = net_cfg;
    net_event_mgr_ = p_net_event_mgr;
    initialized_ = true;
    drained_ = true;
    corrupt_completion_queue_ = false;
    remaining_outstanding_ = 0;
    remaining_active_sessions_ = 0;

    return NetErrorCode::kNONE;
  }

  NetErrorCode Start() {
    if (!initialized_ || started_) {
      return NetErrorCode::kINVALID_CONFIG;
    }
    if (!io_worker_.StartIoThreads(net_config_, rio_completion_queues_)) {
      return NetErrorCode::kIO_THREAD_START_FAIL;
    }
    if (!accepter_.Start(net_config_.session_cfg_, listen_socket_,
                         rio_completion_queues_, session_mgr_, net_event_mgr_,
                         rio_api_)) {
      static_cast<void>(
          io_worker_.Stop(net_config_.graceful_shutdown_timeout_ms_));
      rio_completion_queues_.clear();
      return NetErrorCode::kIO_THREAD_START_FAIL;
    }
    started_ = true;
    return NetErrorCode::kNONE;
  }

  bool End(const bool graceful = false, const std::uint32_t timeout_ms = 5000) {
    if (!initialized_) {
      return true;
    }
    if (started_) {
      accepter_.Stop();
      if (graceful) {
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(timeout_ms);
        while (session_mgr_.SendOutstanding() != 0 &&
               std::chrono::steady_clock::now() < deadline) {
          std::this_thread::yield();
        }
      }
      for (std::uint32_t index = 0;
           index < static_cast<std::uint32_t>(net_config_.max_session_count_);
           ++index) {
        if (auto* session = session_mgr_.GetSessionObj(index);
            session != nullptr && session->HasOpenSocket()) {
          static_cast<void>(
              ForcedClose(static_cast<std::int32_t>(index), session->GetUID(),
                          DisconnectReason::kFORCES_CLOSE_SERVER));
        }
      }
      drained_ = io_worker_.Stop(timeout_ms);
      corrupt_completion_queue_ = io_worker_.corrupt_completion_queue();
      remaining_outstanding_ = session_mgr_.Outstanding();
      remaining_active_sessions_ = session_mgr_.ActiveCount();
    } else {
      listen_socket_.Reset();
    }

    if (!drained_) {
      rio_completion_queues_.clear();
      session_mgr_.AbandonOutstanding();
    }
    session_mgr_.Dispose();
    rio_completion_queues_.clear();
    winsock_runtime_.Reset();
    started_ = false;
    initialized_ = false;

    return drained_ && !corrupt_completion_queue_;
  }

  [[nodiscard]] bool corrupt_completion_queue() const noexcept {
    return corrupt_completion_queue_;
  }

  [[nodiscard]] std::uint64_t remaining_outstanding() const noexcept {
    return remaining_outstanding_;
  }

  [[nodiscard]] std::uint64_t remaining_active_sessions() const noexcept {
    return remaining_active_sessions_;
  }

  NetErrorCode SendData(const std::int32_t session_index,
                        const std::uint64_t session_uid,
                        const std::uint32_t data_size, const char* p_data) {
    if (auto ret = session_mgr_.SendData(session_index, session_uid, data_size,
                                         p_data);
        ret != NetErrorCode::kNONE) {
      if (ret == NetErrorCode::kSEND_IO_FAIL) {
        ForcedClose(session_index, DisconnectReason::kSEND_ERROR);
      }
      return ret;
    }
    return NetErrorCode::kNONE;
  }

  bool ForcedClose(const std::int32_t session_index,
                   const std::uint64_t session_uid,
                   const DisconnectReason reason = DisconnectReason::kACTIVE) {
    if (auto thread_index =
            session_mgr_.ForcedClose(session_index, session_uid);
        thread_index >= 0) {
      lazyed_dis_conn_mgr_.AddSession(thread_index, session_index, session_uid,
                                      reason);
      return true;
    }

    return false;
  }

 private:
  bool DisConnect(TcpSession* p_session, const std::uint64_t session_uid,
                  const DisconnectReason reason) {
    auto session_index = p_session->GetIndex();

    if (reason == DisconnectReason::kACCEPT_LOOP_REQUEST_QUEUE_ERROR ||
        reason == DisconnectReason::kACCEPT_LOOP_FAIL_ON_CONNECT) {
      p_session->DisConnectByAcceptFail();
      session_mgr_.ReturnSession(session_index);
      return true;
    }

    if (reason == DisconnectReason::kZERO_COMPLETION &&
        !p_session->ReadyForPeerClose()) {
      return false;
    }

    const bool abortive = reason != DisconnectReason::kZERO_COMPLETION &&
                          reason != DisconnectReason::kFORCES_CLOSE_SERVER;
    if (p_session->Disconnect(session_uid, abortive)) {
      net_event_mgr_->OnClose(session_index, session_uid, reason);
      session_mgr_.ReturnSession(session_index);
    }
    return true;
  }

  void ForcedClose(const std::int32_t session_index, DisconnectReason reason) {
    // 이 함수를 호출할 때는 절대 session_index는 유효한 값이다.
    auto p_session = session_mgr_.GetSessionObj(session_index);
    p_session->SetDisableNetwork();

    lazyed_dis_conn_mgr_.AddSession(p_session->GetRioThreadId(),
                                    p_session->GetIndex(), p_session->GetUID(),
                                    reason);
  }

  void SetDelegateDisConnect() {
    auto dis_connect_func = [&](TcpSession* p_session,
                                const uint64_t session_uid,
                                const DisconnectReason reason) -> bool {
      return DisConnect(p_session, session_uid, reason);
    };
    io_worker_.DisConnectFunc = dis_connect_func;
    accepter_.DisConnectFunc = dis_connect_func;
    lazyed_dis_conn_mgr_.DisConnectFunc = dis_connect_func;
  }

 private:
  NetConfig net_config_;

  INetEventManager* net_event_mgr_ = nullptr;

  ServerNetLazyedDisConnManager lazyed_dis_conn_mgr_;

  Accepter accepter_;

  ServerNetIOWorker io_worker_;

  std::vector<detail::UniqueRioCompletionQueue> rio_completion_queues_;

  detail::WinsockRuntime winsock_runtime_;
  detail::WindowsRioApi rio_api_;
  detail::WindowsSocketApi socket_api_;
  detail::UniqueSocket listen_socket_;

  SessionManager session_mgr_;

  bool initialized_ = false;
  bool started_ = false;
  bool drained_ = true;
  bool corrupt_completion_queue_ = false;
  std::uint64_t remaining_outstanding_ = 0;
  std::uint64_t remaining_active_sessions_ = 0;
};

}  // namespace rionetlite
