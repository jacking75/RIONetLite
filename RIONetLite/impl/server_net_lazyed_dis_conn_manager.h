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

  void AddSessionQueue() { lazyed_dis_connect_sessions_.emplace_back(); }

  void AddSession(const int thread_index, const INT32 session_index,
                  const UINT64 session_uid, const DisconnectReason reason) {
    lazyed_dis_connect_sessions_[thread_index].push(
        {session_index, session_uid, reason});
  }

  void ProcessLazyedDisConnectSession(const int thread_index,
                                      SessionManager* p_session_mgr) {
    while (true) {
      if (auto disConnectInfo = FetchSession(thread_index);
          disConnectInfo.uid <= 0) {
        break;
      } else {
        auto session_index = disConnectInfo.session_index_;
        auto uid = disConnectInfo.uid;

        if (auto pSession = p_session_mgr->GetSessionObj(session_index, uid);
            pSession != nullptr) {
          DisConnectFunc(pSession, uid, disConnectInfo.reason_);
        } else {
          continue;
        }
      }
    }
  }

  std::function<void(TcpSession*, const uint64_t, const DisconnectReason)>
      DisConnectFunc;

 private:
  LazyedDisConnectSession FetchSession(const int thread_index) {
    LazyedDisConnectSession disConnectInfo;
    lazyed_dis_connect_sessions_[thread_index].try_pop(disConnectInfo);
    return disConnectInfo;
  }

  std::vector<concurrency::concurrent_queue<LazyedDisConnectSession>>
      lazyed_dis_connect_sessions_;
};

}  // namespace rionetlite