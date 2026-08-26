#pragma once

#include <WinSock2.h>

#include "../../src/detail/session_partition.h"
#include "exception.h"
#include "fast_spin_lock.h"
#include "tcp_session.h"

#include <cstdint>
#include <deque>
#include <vector>

namespace rionetlite {
const std::uint64_t kRE_USE_SESSION_WAITE_TIME_SEC = 3;

struct SessionIndexContext {
  std::uint32_t index_ = 0;
  std::uint64_t reuse_able_timesec_ = 0;
};

class SessionManager {
 public:
  SessionManager() = default;
  ~SessionManager() = default;

  bool PrepareSessionPool(NetConfig net_cfg) {
    std::uint32_t client_index = 0;
    auto rioThreadCount = net_cfg.io_work_thread_count_;
    for (decltype(rioThreadCount) i = 0; i < rioThreadCount; ++i) {
      const auto partition = detail::PartitionSessions(
          net_cfg.max_session_count_, rioThreadCount, i);
      for (std::uint32_t j = 0; j < partition.count; ++j) {
        auto client = new TcpSession;

        client->Init(client_index, i);

        if (client->RioInitialize(net_cfg.session_cfg_) == false) {
          client->Dispose();
          delete client;
          return false;
        }

        sessions_.push_back(client);
        free_session_indexs_.emplace_back(SessionIndexContext{
            .index_ = client_index, .reuse_able_timesec_ = 0});

        ++client_index;
      }
    }

    max_session_count_ = client_index;
    return true;
  }

  void Dispose() {
    LogFuncPtr((int)LogLevel::kINFO, "SessionManager::Dispose - Start");

    for (auto pSession : sessions_) {
      if (pSession->IsConnected()) {
        auto uniqueId = pSession->GetUID();
        pSession->Disconnect(uniqueId);  // 네트워크 라이브러리 종료 시에 호출
      }

      pSession->Dispose();
      delete pSession;
    }

    sessions_.clear();
    free_session_indexs_.clear();
    max_session_count_ = 0;

    LogFuncPtr((int)LogLevel::kINFO, "SessionManager::Dispose - Complete");
  }

  TcpSession* GetSessionObj(const std::uint32_t session_index) {
    if (session_index >= max_session_count_) {
      return nullptr;
    }

    return sessions_[session_index];
  }

  TcpSession* GetSessionObj(const std::uint32_t session_index,
                            const std::uint64_t session_uid) {
    if (session_index >= max_session_count_) {
      return nullptr;
    }

    if (sessions_[session_index]->GetUID() != session_uid) {
      return nullptr;
    }

    return sessions_[session_index];
  }

  TcpSession* AllocClientSession(const std::uint64_t cur_sec) {
    FastSpinlockGuard guard(lock_);

    if (free_session_indexs_.empty()) {
      return nullptr;
    }

    auto indexContext = free_session_indexs_.front();

    if (indexContext.reuse_able_timesec_ > cur_sec) {
      return nullptr;
    }

    auto newSession = GetSessionObj(indexContext.index_);
    free_session_indexs_.pop_front();

    // newSessions는 절대 null 객체가 아니다!!!
    return newSession;
  }

  void ReturnSession(const int session_index, const std::uint64_t cur_sec) {
    FastSpinlockGuard guard(lock_);
    free_session_indexs_.emplace_back(
        SessionIndexContext{(std::uint32_t)session_index,
                            (cur_sec + kRE_USE_SESSION_WAITE_TIME_SEC)});
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

    if (session->PostSend(data_size, p_data) == false) {
      return NetErrorCode::kSEND_IO_FAIL;
    }

    return NetErrorCode::kNONE;
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
  std::uint64_t current_issue_count_ = 0;
  std::uint64_t current_return_count_ = 0;

  std::deque<SessionIndexContext> free_session_indexs_;
  std::vector<TcpSession*> sessions_;
};

}  // namespace rionetlite
