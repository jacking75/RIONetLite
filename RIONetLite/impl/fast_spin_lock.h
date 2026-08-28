#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace rionetlite {
// 세션 송신 경로와 세션 풀이 사용하는 짧은 임계 구역용 락이다. SRWLOCK은
// 비경합에서 인터락드 1회로 끝나고 경합 시에도 짧은 스핀 뒤 커널 대기로
// 넘어가므로, 경합마다 타이머 해상도를 바꾸며 Sleep 하던 이전 구현과 달리
// 지연 스파이크를 만들지 않는다. 재귀 획득은 지원하지 않는다.
class FastSpinlock {
 public:
  FastSpinlock() = default;
  ~FastSpinlock() = default;

  FastSpinlock(const FastSpinlock&) = delete;
  FastSpinlock& operator=(const FastSpinlock&) = delete;

  void EnterLock() noexcept { AcquireSRWLockExclusive(&lock_); }

  void LeaveLock() noexcept { ReleaseSRWLockExclusive(&lock_); }

 private:
  SRWLOCK lock_ = SRWLOCK_INIT;
};

class FastSpinlockGuard {
 public:
  explicit FastSpinlockGuard(FastSpinlock& lock) noexcept : lock_(lock) {
    lock_.EnterLock();
  }

  ~FastSpinlockGuard() { lock_.LeaveLock(); }

  FastSpinlockGuard(const FastSpinlockGuard&) = delete;
  FastSpinlockGuard& operator=(const FastSpinlockGuard&) = delete;

 private:
  FastSpinlock& lock_;
};

}  // namespace rionetlite
