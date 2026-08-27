#pragma once

#include "../../src/detail/rio_api.h"
#include "../../src/detail/win32_resources.h"
#include "../define.h"
#include "fast_spin_lock.h"
#include "ring_buffer.h"
#include "rio_context.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

namespace rionetlite {
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
    max_send_count_per_1sec_ = net_cfg.max_send_count_per_1sec_;
    max_send_ring_buffer_size_ = net_cfg.max_send_buffer_size_;

    if (const auto create_result =
            send_buffer_.Create(true, max_send_ring_buffer_size_);
        std::get<0>(create_result) != static_cast<int>(NetErrorCode::kNONE)) {
      return false;
    }

    send_contexts_.resize(max_send_count_per_1sec_);
    for (auto& context : send_contexts_) {
      context.Init(session_index_, IOType::kSEND, owner_worker);
    }

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

    send_contexts_.clear();
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
    if (data_size == 0 ||
        data_size > static_cast<std::uint32_t>(max_send_packet_size_)) {
      return NetErrorCode::kSEND_MESSAGE_TOO_LARGE;
    }
    RioIoContext* send_context = nullptr;
    {
      FastSpinlockGuard guard(lock_);
      if (!accepting_posts_.load(std::memory_order_acquire)) {
        return NetErrorCode::kSEND_SESSION_NETWORK_DISABLE;
      }
      if (session_uid == 0 || session_uid != active_session_uid_) {
        return NetErrorCode::kSEND_INVALID_SESSION;
      }
      std::size_t current_position = 0;
      for (std::size_t offset = 0; offset < send_contexts_.size(); ++offset) {
        current_position = (send_sequence_ + offset) % send_contexts_.size();
        if (send_contexts_[current_position].IsIdle()) {
          send_context = &send_contexts_[current_position];
          send_sequence_ = current_position + 1;
          break;
        }
      }
      if (send_context == nullptr) {
        return NetErrorCode::kSEND_THREAD_BUFFER_FULL;
      }

      const auto writable_position = static_cast<std::uint32_t>(
          current_position * static_cast<std::size_t>(max_send_packet_size_));
      std::memcpy(send_buffer_.GetBuffer() + writable_position, p_data,
                  data_size);

      if (!send_context->BeginRequest(session_uid, send_rio_buffer_.get(),
                                      writable_position, data_size,
                                      send_buffer_.RegisteredSpanSize())) {
        return NetErrorCode::kSEND_THREAD_BUFFER_FULL;
      }
      send_rio_buffer_.AddOutstanding();
      posts_in_flight_.fetch_add(1, std::memory_order_acq_rel);
    }

    if (!rio_api_->Send(rio_rq_, reinterpret_cast<PRIO_BUF>(send_context), 1, 0,
                        send_context)) {
      FinishPost();
      Failed(send_context);
      return NetErrorCode::kSEND_IO_FAIL;
    }
    FinishPost();

    return NetErrorCode::kNONE;
  }

  bool Completed(RioIoContext* send_context, const std::uint32_t transferred) {
    {
      FastSpinlockGuard guard(lock_);
      if (!send_context->CompleteSend(transferred)) {
        FailUnlocked(send_context);
        return false;
      }
      if (send_context->IsIdle()) {
        send_rio_buffer_.CompleteOutstanding();
        return true;
      }
      if (!accepting_posts_.load(std::memory_order_acquire)) {
        FailUnlocked(send_context);
        return false;
      }
      posts_in_flight_.fetch_add(1, std::memory_order_acq_rel);
    }
    const bool posted = rio_api_->Send(
        rio_rq_, reinterpret_cast<PRIO_BUF>(send_context), 1, 0, send_context);
    FinishPost();
    if (!posted) {
      Failed(send_context);
      return false;
    }
    return true;
  }

  void Failed(RioIoContext* send_context) {
    FastSpinlockGuard guard(lock_);
    FailUnlocked(send_context);
  }

  std::uint32_t Outstanding() const noexcept {
    return send_rio_buffer_.outstanding();
  }

  void AbandonOutstanding() noexcept {
    BeginClose();
    FastSpinlockGuard guard(lock_);
    for (auto& context : send_contexts_) {
      context.Fail();
    }
    send_rio_buffer_.AbandonOutstanding();
  }

 private:
  void Clear() {
    FastSpinlockGuard guard(lock_);
    ClearUnlocked();
  }

  void ClearUnlocked() {
    for (auto& context : send_contexts_) {
      context.Fail();
    }

    send_buffer_.Clear();
  }

  void FailUnlocked(RioIoContext* send_context) noexcept {
    if (!send_context->IsIdle()) {
      send_context->Fail();
      send_rio_buffer_.CompleteOutstanding();
    }
  }

  void FinishPost() noexcept {
    if (posts_in_flight_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      posts_in_flight_.notify_all();
    }
  }

  std::int32_t session_index_ = 0;
  std::int32_t max_send_packet_size_ = 0;
  std::uint64_t max_send_count_per_1sec_ = 0;
  std::int32_t max_send_ring_buffer_size_ = 0;

  std::uint64_t send_sequence_ = 0;
  std::uint64_t active_session_uid_ = 0;
  std::vector<RioIoContext> send_contexts_;

  RIO_RQ rio_rq_ = RIO_INVALID_RQ;
  RingBufferVA2 send_buffer_;
  detail::RegisteredRioBuffer send_rio_buffer_;
  detail::RioApi* rio_api_ = nullptr;
  FastSpinlock lock_;
  std::atomic<bool> accepting_posts_{false};
  std::atomic<std::uint32_t> posts_in_flight_{0};
};

}  // namespace rionetlite
