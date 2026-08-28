#pragma once

#include <WinSock2.h>

#include "../../src/detail/rio_api.h"
#include "../../src/detail/session_partition.h"
#include "fast_spin_lock.h"
#include "tcp_session.h"

#include <cassert>
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

namespace rionetlite {
struct SessionIndexContext {
  std::uint32_t index_ = 0;
};

enum class SessionPoolState : std::uint8_t {
  kFree,
  kLeased,
  kQuarantined,
};

class SessionManager {
 public:
  SessionManager() = default;
  ~SessionManager() = default;

  bool PrepareSessionPool(NetConfig net_cfg, detail::RioApi& rio_api) {
    std::uint32_t client_index = 0;
    auto rioThreadCount = net_cfg.io_work_thread_count_;
    worker_partitions_.clear();
    worker_partitions_.reserve(static_cast<std::size_t>(rioThreadCount));
    for (decltype(rioThreadCount) i = 0; i < rioThreadCount; ++i) {
      const auto partition = detail::PartitionSessions(
          net_cfg.max_session_count_, rioThreadCount, i);
      worker_partitions_.push_back(partition);
      for (std::uint32_t j = 0; j < partition.count; ++j) {
        auto client = std::make_unique<TcpSession>(client_index, i);

        if (client->RioInitialize(net_cfg.session_cfg_, rio_api) == false) {
          client->Dispose();
          Dispose();
          return false;
        }

        sessions_.push_back(std::move(client));
        pool_states_.push_back(SessionPoolState::kFree);

        ++client_index;
      }
    }

    max_session_count_ = client_index;
    BuildInterleavedFreeList();
    return true;
  }

  void Dispose() {
    for (const auto& session : sessions_) {
      if (session->IsConnected()) {
        auto uniqueId = session->GetUID();
        session->Disconnect(uniqueId);  // 네트워크 라이브러리 종료 시에 호출
      }

      session->Dispose();
    }

    sessions_.clear();
    free_session_indexs_.clear();
    quarantined_session_indexs_.clear();
    pool_states_.clear();
    worker_partitions_.clear();
    max_session_count_ = 0;
  }

  TcpSession* GetSessionObj(const std::uint32_t session_index) {
    if (session_index >= max_session_count_) {
      return nullptr;
    }

    return sessions_[session_index].get();
  }

  TcpSession* GetSessionObj(const std::uint32_t session_index,
                            const std::uint64_t session_uid) {
    if (session_index >= max_session_count_) {
      return nullptr;
    }

    if (sessions_[session_index]->GetUID() != session_uid) {
      return nullptr;
    }

    return sessions_[session_index].get();
  }

  TcpSession* AllocClientSession() {
    FastSpinlockGuard guard(lock_);

    for (auto iterator = quarantined_session_indexs_.begin();
         iterator != quarantined_session_indexs_.end();) {
      auto* session = GetSessionObj(iterator->index_);
      if (session != nullptr &&
          pool_states_[iterator->index_] == SessionPoolState::kQuarantined &&
          session->TryRecycle()) {
        pool_states_[iterator->index_] = SessionPoolState::kFree;
        free_session_indexs_.push_back(*iterator);
        iterator = quarantined_session_indexs_.erase(iterator);
      } else {
        ++iterator;
      }
    }

    if (free_session_indexs_.empty()) {
      return nullptr;
    }

    auto indexContext = free_session_indexs_.front();

    auto newSession = GetSessionObj(indexContext.index_);
    free_session_indexs_.pop_front();

    assert(indexContext.index_ < pool_states_.size());
    assert(pool_states_[indexContext.index_] == SessionPoolState::kFree);
    if (newSession == nullptr || !newSession->TryReserve()) {
      if (newSession != nullptr) {
        free_session_indexs_.push_front(indexContext);
      }
      return nullptr;
    }
    pool_states_[indexContext.index_] = SessionPoolState::kLeased;
    return newSession;
  }

  void ReturnSession(const int session_index) {
    FastSpinlockGuard guard(lock_);
    if (session_index < 0 ||
        static_cast<std::uint32_t>(session_index) >= pool_states_.size()) {
      assert(false && "session index is outside the pool");
      return;
    }
    const auto index = static_cast<std::uint32_t>(session_index);
    if (pool_states_[index] != SessionPoolState::kLeased) {
      assert(false && "session slot was returned more than once");
      return;
    }
    auto* session = GetSessionObj(index);
    SessionIndexContext context{index};
    if (session != nullptr && session->TryRecycle()) {
      pool_states_[index] = SessionPoolState::kFree;
      free_session_indexs_.push_back(context);
    } else {
      pool_states_[index] = SessionPoolState::kQuarantined;
      quarantined_session_indexs_.push_back(context);
    }
  }

  std::uint64_t Outstanding() const noexcept {
    std::uint64_t total = 0;
    for (const auto& session : sessions_) {
      total += session->Outstanding();
    }
    return total;
  }

  std::uint64_t SendOutstanding() const noexcept {
    std::uint64_t total = 0;
    for (const auto& session : sessions_) {
      total += session->SendOutstanding();
    }
    return total;
  }

  std::uint64_t ActiveCount() const noexcept {
    std::uint64_t total = 0;
    for (const auto& session : sessions_) {
      if (session->HasOpenSocket()) {
        ++total;
      }
    }
    return total;
  }

  // 종료 대기 루프용 조건이다. 첫 활성 세션에서 즉시 반환하므로 세션 수가 큰
  // 구성에서도 매 반복 전체 풀을 합산하지 않는다.
  [[nodiscard]] bool AnyOutstandingOrActive() const noexcept {
    for (const auto& session : sessions_) {
      if (session->Outstanding() != 0 || session->HasOpenSocket()) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] bool AnyOutstanding() const noexcept {
    for (const auto& session : sessions_) {
      if (session->Outstanding() != 0) {
        return true;
      }
    }
    return false;
  }

  // 종료 타임아웃 경로에서 커널이 미완료 요청을 취소하도록 소켓만 먼저 닫는다.
  void AbortAllSockets() noexcept {
    for (const auto& session : sessions_) {
      session->AbortSocket();
    }
  }

  // 드레인에 실패한 경우에만 사용한다. 커널이 여전히 참조할 수 있는 등록
  // 버퍼와 매핑을 해제하지 않고 소유권만 포기해 use-after-free를 막는다.
  void LeakRegisteredResources() noexcept {
    for (const auto& session : sessions_) {
      session->LeakRegisteredResources();
    }
  }

  [[nodiscard]] detail::SessionPartition WorkerPartition(
      const std::uint32_t worker_index) const noexcept {
    if (worker_index >= worker_partitions_.size()) {
      return {};
    }
    return worker_partitions_[worker_index];
  }

  NetErrorCode SendData(const std::int32_t session_index,
                        const std::uint64_t session_uid,
                        const std::uint32_t data_size, const char* p_data) {
    auto session = GetSessionObj(session_index, session_uid);
    if (session == nullptr) {
      return NetErrorCode::kSEND_INVALID_SESSION;
    }

    if (session->IsEnableNetwork() == false) {
      return NetErrorCode::kSEND_SESSION_NETWORK_DISABLE;
    }

    return session->PostSend(session_uid, data_size, p_data);
  }

  NetErrorCode SendData(const std::int32_t session_index,
                        const std::uint64_t session_uid,
                        const std::uint32_t header_size, const char* p_header,
                        const std::uint32_t payload_size,
                        const char* p_payload) {
    auto session = GetSessionObj(session_index, session_uid);
    if (session == nullptr) {
      return NetErrorCode::kSEND_INVALID_SESSION;
    }

    if (session->IsEnableNetwork() == false) {
      return NetErrorCode::kSEND_SESSION_NETWORK_DISABLE;
    }

    return session->PostSend(session_uid, header_size, p_header, payload_size,
                             p_payload);
  }

  int ForcedClose(const std::int32_t session_index,
                  const std::uint64_t session_uid) {
    auto session = GetSessionObj(session_index, session_uid);
    if (session == nullptr) {
      return -1;
    }

    session->SetDisableNetwork();

    auto threadIndex = session->GetRioThreadId();
    return threadIndex;
  }

 private:
  // 초기 free list를 worker 파티션 순서대로 채우면 기동 직후의 첫 접속들이
  // 모두 worker 0에 배정된다. 라운드로빈으로 섞어 cold-start 편중을 없앤다.
  void BuildInterleavedFreeList() {
    free_session_indexs_.clear();
    for (std::uint32_t offset = 0;; ++offset) {
      bool pushed = false;
      for (const auto& partition : worker_partitions_) {
        if (offset < partition.count) {
          free_session_indexs_.emplace_back(
              SessionIndexContext{.index_ = partition.first_index + offset});
          pushed = true;
        }
      }
      if (!pushed) {
        break;
      }
    }
  }

  FastSpinlock lock_;

  std::uint32_t max_session_count_ = 0;
  std::deque<SessionIndexContext> free_session_indexs_;
  std::deque<SessionIndexContext> quarantined_session_indexs_;
  std::vector<SessionPoolState> pool_states_;
  std::vector<detail::SessionPartition> worker_partitions_;
  std::vector<std::unique_ptr<TcpSession>> sessions_;
};

}  // namespace rionetlite
