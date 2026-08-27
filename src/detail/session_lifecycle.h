#pragma once

#include "rionetlite/event_handler.h"

#include <atomic>
#include <cstdint>

namespace rionetlite::detail {

enum class SessionState : std::uint8_t {
  kFree,
  kAccepting,
  kActive,
  kDraining,
  kClosing,
  kQuarantined,
};

class SessionLifecycle final {
 public:
  explicit SessionLifecycle(std::uint32_t index,
                            std::uint64_t initial_generation = 0) noexcept
      : index_(index), generation_(initial_generation) {}

  [[nodiscard]] bool TryReserve() noexcept {
    SessionState expected = SessionState::kFree;
    if (!state_.compare_exchange_strong(expected, SessionState::kAccepting,
                                        std::memory_order_acq_rel)) {
      return false;
    }
    std::uint64_t next =
        generation_.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (next == 0) {
      generation_.store(1, std::memory_order_release);
    }
    close_notified_.store(false, std::memory_order_release);
    return true;
  }

  [[nodiscard]] bool TryActivate() noexcept {
    return Transition(SessionState::kAccepting, SessionState::kActive);
  }

  [[nodiscard]] bool TryBeginClose() noexcept {
    SessionState current = state_.load(std::memory_order_acquire);
    while (current == SessionState::kAccepting ||
           current == SessionState::kActive) {
      if (state_.compare_exchange_weak(current, SessionState::kDraining,
                                       std::memory_order_acq_rel)) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] bool TryMarkClosing() noexcept {
    return Transition(SessionState::kDraining, SessionState::kClosing);
  }

  [[nodiscard]] bool TryNotifyClose() noexcept {
    bool expected = false;
    return close_notified_.compare_exchange_strong(expected, true,
                                                   std::memory_order_acq_rel);
  }

  [[nodiscard]] bool TryQuarantine() noexcept {
    return Transition(SessionState::kClosing, SessionState::kQuarantined);
  }

  [[nodiscard]] bool TryRecycle(std::uint32_t outstanding_io,
                                std::uint32_t pending_events) noexcept {
    if (outstanding_io != 0 || pending_events != 0 ||
        !close_notified_.load(std::memory_order_acquire)) {
      return false;
    }
    return Transition(SessionState::kQuarantined, SessionState::kFree);
  }

  [[nodiscard]] bool Matches(SessionId session) const noexcept {
    return session.index == index_ && session.generation != 0 &&
           session.generation == generation_.load(std::memory_order_acquire);
  }

  [[nodiscard]] SessionId id() const noexcept {
    return SessionId{.index = index_,
                     .generation = generation_.load(std::memory_order_acquire)};
  }

  [[nodiscard]] SessionState state() const noexcept {
    return state_.load(std::memory_order_acquire);
  }

 private:
  bool Transition(SessionState from, SessionState to) noexcept {
    return state_.compare_exchange_strong(from, to, std::memory_order_acq_rel);
  }

  const std::uint32_t index_;
  std::atomic<std::uint64_t> generation_{0};
  std::atomic<SessionState> state_{SessionState::kFree};
  std::atomic<bool> close_notified_{false};
};

}  // namespace rionetlite::detail
