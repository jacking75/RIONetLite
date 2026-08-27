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
    for (decltype(rioThreadCount) i = 0; i < rioThreadCount; ++i) {
      const auto partition = detail::PartitionSessions(
          net_cfg.max_session_count_, rioThreadCount, i);
      for (std::uint32_t j = 0; j < partition.count; ++j) {
        auto client = std::make_unique<TcpSession>(client_index, i);

        if (client->RioInitialize(net_cfg.session_cfg_, rio_api) == false) {
          client->Dispose();
          Dispose();
          return false;
        }

        sessions_.push_back(std::move(client));
        free_session_indexs_.emplace_back(
            SessionIndexContext{.index_ = client_index});
        pool_states_.push_back(SessionPoolState::kFree);

        ++client_index;
      }
    }

    max_session_count_ = client_index;
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

  void AbandonOutstanding() noexcept {
    for (const auto& session : sessions_) {
      session->AbandonOutstanding();
    }
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
  FastSpinlock lock_;

  std::uint32_t max_session_count_ = 0;
  std::deque<SessionIndexContext> free_session_indexs_;
  std::deque<SessionIndexContext> quarantined_session_indexs_;
  std::vector<SessionPoolState> pool_states_;
  std::vector<std::unique_ptr<TcpSession>> sessions_;
};

}  // namespace rionetlite
