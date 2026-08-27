#pragma once

#include "rionetlite/server.h"

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace rionetlite::detail {

class ShutdownCoordinator final {
 public:
  [[nodiscard]] bool Begin(const StopMode mode) noexcept {
    bool expected = false;
    if (!stopping_.compare_exchange_strong(expected, true,
                                           std::memory_order_acq_rel)) {
      return false;
    }
    immediate_.store(mode == StopMode::kImmediate, std::memory_order_release);
    return true;
  }

  void Complete() noexcept {
    {
      std::scoped_lock lock(mutex_);
      completed_.store(true, std::memory_order_release);
    }
    completed_cv_.notify_all();
  }

  void Wait() noexcept {
    std::unique_lock lock(mutex_);
    completed_cv_.wait(
        lock, [this] { return completed_.load(std::memory_order_acquire); });
  }

  [[nodiscard]] bool stopping() const noexcept {
    return stopping_.load(std::memory_order_acquire);
  }
  [[nodiscard]] bool immediate() const noexcept {
    return immediate_.load(std::memory_order_acquire);
  }
  [[nodiscard]] bool completed() const noexcept {
    return completed_.load(std::memory_order_acquire);
  }

 private:
  std::atomic<bool> stopping_{false};
  std::atomic<bool> immediate_{false};
  std::atomic<bool> completed_{false};
  std::mutex mutex_;
  std::condition_variable completed_cv_;
};

}  // namespace rionetlite::detail
