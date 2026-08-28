#pragma once

#include "../define.h"
#include "session_manager.h"

#include <concurrent_queue.h>
#include <cstdint>
#include <functional>
#include <vector>

namespace rionetlite {
// 주로 서버에거 강제로 접속을 짜를 때 사용된다.
// 접속을 짜를 세션을 처리하는 i/o 스레드에서 접속을 짜르도록 한다.
class ServerNetLazyedDisConnManager {
 public:
  ServerNetLazyedDisConnManager() = default;
  ~ServerNetLazyedDisConnManager() = default;

  void PrepareSessionQueues(const std::uint32_t thread_count,
                            const std::uint32_t drain_timeout_ms) {
    drain_timeout_ms_ = drain_timeout_ms;
    lazyed_dis_connect_sessions_.clear();
    for (std::uint32_t i = 0; i < thread_count; ++i) {
      lazyed_dis_connect_sessions_.emplace_back();
    }
  }

  void AddSession(const int thread_index, const INT32 session_index,
                  const UINT64 session_uid, const DisconnectReason reason) {
    lazyed_dis_connect_sessions_[thread_index].push(
        {session_index, session_uid, reason, 0});
  }

  void ProcessLazyedDisConnectSession(const int thread_index,
                                      SessionManager* p_session_mgr) {
    std::vector<LazyedDisConnectSession> deferred;
    const std::uint64_t now = GetTickCount64();
    while (true) {
      if (auto disConnectInfo = FetchSession(thread_index);
          disConnectInfo.uid <= 0) {
        break;
      } else {
        auto session_index = disConnectInfo.session_index_;
        auto uid = disConnectInfo.uid;

        if (auto pSession = p_session_mgr->GetSessionObj(session_index, uid);
            pSession != nullptr) {
          // peer가 FIN 직후 사라지면 미완료 송신이 TCP 재전송 타임아웃까지
          // 완료되지 않는다. 마감을 넘기면 abortive로 강등해 슬롯을 회수한다.
          if (disConnectInfo.drain_deadline_ms_ != 0 &&
              now >= disConnectInfo.drain_deadline_ms_) {
            disConnectInfo.reason_ = DisconnectReason::kSEND_ERROR;
            disConnectInfo.drain_deadline_ms_ = 0;
          }
          if (!DisConnectFunc(pSession, uid, disConnectInfo.reason_)) {
            if (disConnectInfo.drain_deadline_ms_ == 0) {
              disConnectInfo.drain_deadline_ms_ = now + drain_timeout_ms_;
            }
            deferred.push_back(disConnectInfo);
          }
        } else {
          continue;
        }
      }
    }
    for (const auto& item : deferred) {
      lazyed_dis_connect_sessions_[thread_index].push(item);
    }
  }

  std::function<bool(TcpSession*, const uint64_t, const DisconnectReason)>
      DisConnectFunc;

 private:
  LazyedDisConnectSession FetchSession(const int thread_index) {
    LazyedDisConnectSession disConnectInfo;
    lazyed_dis_connect_sessions_[thread_index].try_pop(disConnectInfo);
    return disConnectInfo;
  }

  std::uint32_t drain_timeout_ms_ = 5000;
  std::vector<concurrency::concurrent_queue<LazyedDisConnectSession>>
      lazyed_dis_connect_sessions_;
};

}  // namespace rionetlite
