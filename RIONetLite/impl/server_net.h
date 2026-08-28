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

#include <algorithm>
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
    if (!accepter_.Start(listen_socket_, rio_completion_queues_, session_mgr_,
                         net_event_mgr_, rio_api_)) {
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
      if (!drained_ || corrupt_completion_queue_) {
        ForceDrainAfterTimeout();
      } else {
        remaining_outstanding_ = session_mgr_.Outstanding();
        remaining_active_sessions_ = session_mgr_.ActiveCount();
      }
    } else {
      listen_socket_.Reset();
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
        // 세대를 함께 전달해야 실패와 종료 사이에 슬롯이 재사용된 경우
        // 새 세션을 끊지 않는다.
        static_cast<void>(ForcedClose(session_index, session_uid,
                                      DisconnectReason::kSEND_ERROR));
      }
      return ret;
    }
    return NetErrorCode::kNONE;
  }

  NetErrorCode SendData(const std::int32_t session_index,
                        const std::uint64_t session_uid,
                        const std::uint32_t header_size, const char* p_header,
                        const std::uint32_t payload_size,
                        const char* p_payload) {
    if (auto ret =
            session_mgr_.SendData(session_index, session_uid, header_size,
                                  p_header, payload_size, p_payload);
        ret != NetErrorCode::kNONE) {
      if (ret == NetErrorCode::kSEND_IO_FAIL) {
        static_cast<void>(ForcedClose(session_index, session_uid,
                                      DisconnectReason::kSEND_ERROR));
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
  // worker가 시간 안에 드레인하지 못했을 때의 안전한 회수 순서다. 소켓을 먼저
  // 닫아 커널이 미완료 요청을 취소하게 하고, 취소 완료를 이 스레드에서 짧게
  // 드레인한다. 그래도 남으면 커널이 참조 중인 등록 버퍼·매핑·CQ를 해제하지
  // 않고 의도적으로 누수시켜 use-after-free를 막는다.
  void ForceDrainAfterTimeout() {
    session_mgr_.AbortAllSockets();

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(kForcedDrainTimeoutMs);
    std::vector<RIORESULT> results(
        std::max<std::size_t>(1, net_config_.completion_batch_size_));
    bool corrupt = false;
    while (!corrupt && session_mgr_.AnyOutstanding() &&
           std::chrono::steady_clock::now() < deadline) {
      bool progressed = false;
      for (auto& queue : rio_completion_queues_) {
        if (!queue) {
          continue;
        }
        const ULONG count = rio_api_.DequeueCompletion(
            queue.get(), results.data(), static_cast<ULONG>(results.size()));
        if (count == RIO_CORRUPT_CQ) {
          corrupt = true;
          corrupt_completion_queue_ = true;
          break;
        }
        for (ULONG index = 0; index < count; ++index) {
          RetireCompletion(results[index]);
        }
        progressed = progressed || count != 0;
      }
      if (!progressed) {
        std::this_thread::yield();
      }
    }

    remaining_outstanding_ = session_mgr_.Outstanding();
    remaining_active_sessions_ = session_mgr_.ActiveCount();

    if (remaining_outstanding_ != 0) {
      session_mgr_.LeakRegisteredResources();
      for (auto& queue : rio_completion_queues_) {
        queue.Release();
      }
    }
  }

  void RetireCompletion(const RIORESULT& result) {
    auto* context = reinterpret_cast<RioIoContext*>(result.RequestContext);
    if (context == nullptr) {
      return;
    }
    auto* session = session_mgr_.GetSessionObj(
        static_cast<std::uint32_t>(context->session_index_));
    if (session == nullptr) {
      return;
    }
    if (context->io_type_ == IOType::kRECV) {
      session->RecvFailed();
    } else if (context->io_type_ == IOType::kSEND) {
      session->SendFailed(context);
    }
  }

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
  static constexpr std::uint32_t kForcedDrainTimeoutMs = 500;

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
