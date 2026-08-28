#pragma once

#include "../../src/detail/rio_api.h"
#include "../../src/detail/win32_resources.h"
#include "../define.h"
#include "fast_spin_lock.h"
#include "ring_buffer.h"
#include "rio_context.h"

#include <atomic>
#include <cstdint>
#include <tuple>

namespace rionetlite {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif

// 세션당 in-flight 송신 요청을 1개로 고정하고, 애플리케이션이 쌓은 바이트를
// 완료 시점에 하나의 RIO_BUF로 묶어 다시 게시한다. 링이 double-mapped라
// wrap 구간도 연속 주소로 게시할 수 있어 추가 복사가 없다. 배압은 슬롯 수가
// 아니라 링에 남은 바이트로 판정한다.
class SendImmediately {
 public:
  SendImmediately() = default;
  ~SendImmediately() = default;

  char* GetBuffer() { return send_buffer_.GetBuffer(); }

  bool Init(const std::int32_t session_index, SessionNetConfig net_cfg,
            detail::RioApi& rio_api, const std::uint32_t owner_worker = 0) {
    rio_api_ = &rio_api;
    session_index_ = session_index;
    max_send_packet_size_ = net_cfg.max_send_packet_size_;
    max_send_ring_buffer_size_ = net_cfg.max_send_buffer_size_;

    if (const auto create_result =
            send_buffer_.Create(max_send_ring_buffer_size_);
        std::get<0>(create_result) != static_cast<int>(NetErrorCode::kNONE)) {
      return false;
    }

    send_context_.Init(session_index_, IOType::kSEND, owner_worker);

    if (!send_rio_buffer_.Register(*rio_api_, send_buffer_.GetBuffer(),
                                   send_buffer_.RegisteredSpanSize())) {
      return false;
    }

    Clear();

    return true;
  }

  void Dispose() {
    BeginClose();
    static_cast<void>(send_rio_buffer_.Reset());
  }

  void OnConnect(RIO_RQ request_queue, const std::uint64_t session_uid) {
    FastSpinlockGuard guard(lock_);
    rio_rq_ = request_queue;
    ClearUnlocked();
    active_session_uid_ = session_uid;
    accepting_posts_.store(true, std::memory_order_release);
  }

  void BeginClose() noexcept {
    {
      FastSpinlockGuard guard(lock_);
      accepting_posts_.store(false, std::memory_order_release);
    }
    auto posts = posts_in_flight_.load(std::memory_order_acquire);
    while (posts != 0) {
      posts_in_flight_.wait(posts, std::memory_order_acquire);
      posts = posts_in_flight_.load(std::memory_order_acquire);
    }
  }

  NetErrorCode PostSend(const std::uint64_t session_uid,
                        const std::uint32_t data_size, const char* p_data) {
    return PostSendScatter(session_uid, data_size, p_data, 0, nullptr);
  }

  NetErrorCode PostSend(const std::uint64_t session_uid,
                        const std::uint32_t header_size, const char* p_header,
                        const std::uint32_t payload_size,
                        const char* p_payload) {
    return PostSendScatter(session_uid, header_size, p_header, payload_size,
                           p_payload);
  }

  bool Completed(RioIoContext* send_context, const std::uint32_t transferred) {
    {
      FastSpinlockGuard guard(lock_);
      if (send_context != &send_context_ ||
          !send_context_.CompleteSend(transferred)) {
        FailUnlocked();
        return false;
      }
      send_context_.Retire();
      send_rio_buffer_.CompleteOutstanding();
      in_flight_ = false;
      send_buffer_.IncremantReadPos(transferred);

      if (send_buffer_.GetUsedSize() == 0 ||
          !accepting_posts_.load(std::memory_order_acquire)) {
        return true;
      }
      if (!PreparePostLocked()) {
        return false;
      }
    }
    return IssuePost() == NetErrorCode::kNONE;
  }

  void Failed(RioIoContext* send_context) {
    FastSpinlockGuard guard(lock_);
    if (send_context != nullptr && send_context != &send_context_) {
      return;
    }
    FailUnlocked();
  }

  std::uint32_t Outstanding() const noexcept {
    return send_rio_buffer_.outstanding();
  }

  // 종료 타임아웃처럼 완료를 더 이상 기대할 수 없는 경로에서만 사용한다.
  void AbandonOutstanding() noexcept {
    BeginClose();
    FastSpinlockGuard guard(lock_);
    send_context_.Fail();
    in_flight_ = false;
    send_rio_buffer_.AbandonOutstanding();
  }

  void LeakRegisteredResources() noexcept {
    send_rio_buffer_.Release();
    send_buffer_.ReleaseStorage();
  }

 private:
  NetErrorCode PostSendScatter(const std::uint64_t session_uid,
                               const std::uint32_t first_size,
                               const char* p_first,
                               const std::uint32_t second_size,
                               const char* p_second) {
    const std::uint64_t total = static_cast<std::uint64_t>(first_size) +
                                static_cast<std::uint64_t>(second_size);
    if (total == 0 ||
        total > static_cast<std::uint64_t>(max_send_packet_size_)) {
      return NetErrorCode::kSEND_MESSAGE_TOO_LARGE;
    }
    const auto total_size = static_cast<std::uint32_t>(total);

    {
      FastSpinlockGuard guard(lock_);
      if (!accepting_posts_.load(std::memory_order_acquire)) {
        return NetErrorCode::kSEND_SESSION_NETWORK_DISABLE;
      }
      if (session_uid == 0 || session_uid != active_session_uid_) {
        return NetErrorCode::kSEND_INVALID_SESSION;
      }
      if (send_buffer_.GetWritableSize() < total_size) {
        return NetErrorCode::kSEND_THREAD_BUFFER_FULL;
      }
      if (first_size != 0 &&
          send_buffer_.Write(first_size, p_first) == nullptr) {
        return NetErrorCode::kSEND_THREAD_BUFFER_FULL;
      }
      if (second_size != 0 &&
          send_buffer_.Write(second_size, p_second) == nullptr) {
        return NetErrorCode::kSEND_THREAD_BUFFER_FULL;
      }

      // 이미 요청이 진행 중이면 완료 처리가 이어서 게시한다.
      if (in_flight_) {
        return NetErrorCode::kNONE;
      }
      if (!PreparePostLocked()) {
        return NetErrorCode::kSEND_IO_FAIL;
      }
    }

    return IssuePost();
  }

  // lock을 보유한 상태에서 링에 쌓인 전체 구간을 하나의 요청으로 준비한다.
  bool PreparePostLocked() {
    if (!send_context_.BeginRequest(active_session_uid_, send_rio_buffer_.get(),
                                    send_buffer_.GetReadPos(),
                                    send_buffer_.GetUsedSize(),
                                    send_buffer_.RegisteredSpanSize())) {
      return false;
    }
    send_rio_buffer_.AddOutstanding();
    in_flight_ = true;
    posts_in_flight_.fetch_add(1, std::memory_order_acq_rel);
    return true;
  }

  // syscall을 lock 밖에서 수행한다. posts_in_flight_가 rio_rq_ 수명을 보호한다.
  NetErrorCode IssuePost() {
    const bool posted =
        rio_api_->Send(rio_rq_, reinterpret_cast<PRIO_BUF>(&send_context_), 1,
                       0, &send_context_);
    FinishPost();
    if (!posted) {
      Failed(&send_context_);
      return NetErrorCode::kSEND_IO_FAIL;
    }
    return NetErrorCode::kNONE;
  }

  void Clear() {
    FastSpinlockGuard guard(lock_);
    ClearUnlocked();
  }

  void ClearUnlocked() {
    send_context_.Fail();
    in_flight_ = false;
    send_buffer_.Clear();
  }

  void FailUnlocked() noexcept {
    if (!send_context_.IsIdle()) {
      send_context_.Fail();
      send_rio_buffer_.CompleteOutstanding();
    }
    in_flight_ = false;
  }

  void FinishPost() noexcept {
    if (posts_in_flight_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      posts_in_flight_.notify_all();
    }
  }

  std::int32_t session_index_ = 0;
  std::int32_t max_send_packet_size_ = 0;
  std::int32_t max_send_ring_buffer_size_ = 0;

  std::uint64_t active_session_uid_ = 0;
  bool in_flight_ = false;
  RioIoContext send_context_{};

  RIO_RQ rio_rq_ = RIO_INVALID_RQ;
  RingBufferVA2 send_buffer_;
  detail::RegisteredRioBuffer send_rio_buffer_;
  detail::RioApi* rio_api_ = nullptr;
  FastSpinlock lock_;
  std::atomic<bool> accepting_posts_{false};
  std::atomic<std::uint32_t> posts_in_flight_{0};
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}  // namespace rionetlite
