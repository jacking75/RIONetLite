#pragma once

#include "..\define.h"
#include "..\i_net_event.h"
#include "helper.h"
#include "send_manager.h"
#include "server_net_accepter.h"
#include "server_net_init.h"
#include "server_net_io_worker.h"
#include "server_net_lazyed_dis_conn_manager.h"
#include "session_manager.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rionetlite {
class ServerNetManagerImpl {
 public:
  ServerNetManagerImpl() = default;
  ~ServerNetManagerImpl() = default;

  NetErrorCode Initialize(NetConfig net_cfg,
                          INetEventManager* p_net_event_mgr) {
    if (auto ret = ServerNetManagerInit::Initialize(listen_socket_, net_cfg);
        ret != NetErrorCode::kNONE) {
      return ret;
    }

    if (session_mgr_.PrepareSessionPool(net_cfg) == false) {
      return NetErrorCode::kPREPARE_SESSION_POOL_FAIL;
    }

    io_worker_.Init(&lazyed_dis_conn_mgr_, &session_mgr_, p_net_event_mgr,
                    &send_mgr_);

    SetDelegateDisConnect();

    net_config_ = net_cfg;
    net_event_mgr_ = p_net_event_mgr;

    LogFuncPtr((int)LogLevel::kINFO,
               "ServerNetManagerImpl::Initialize() - Success!");
    return NetErrorCode::kNONE;
  }

  void Start() {
    io_worker_.StartIoThreads(net_config_.io_work_thread_count_,
                              net_config_.session_cfg_.max_send_count_per_1sec_,
                              net_config_.max_session_count_,
                              net_config_.io_work_thread_count_,
                              rio_completion_queues_);
#ifdef __USE_SEND_THREAD__
    if (StartSendThread(net_config_) == false) {
      LogFuncPtr(
          (int)LogLevel::kFATAL,
          "ServerNetManagerImpl::StartAndBlocking() - Fail StartSendThread");
      ::abort();
    }
#endif  // __USE_SEND_THREAD__

    accepter_.Start(net_config_.session_cfg_, listen_socket_,
                    rio_completion_queues_, session_mgr_, net_event_mgr_,
                    &send_mgr_);
  }

  void End() {
    LogFuncPtr((int)LogLevel::kINFO, "ServerNetManager::End - Start");

    accepter_.Stop();

    io_worker_.Stop();

#ifdef __USE_SEND_THREAD__
    send_mgr_.End();
#endif

    session_mgr_.Dispose();

    WSACleanup();

    LogFuncPtr((int)LogLevel::kINFO, "ServerNetManager::End - Complete");
  }

  NetErrorCode SendData(const std::int32_t session_index,
                        const std::uint64_t session_uid,
                        const std::uint32_t data_size, const char* p_data) {
#ifndef __USE_SEND_THREAD__
    if (auto ret = session_mgr_.SendData(session_index, session_uid, data_size,
                                         p_data);
        ret != NetErrorCode::kNONE) {
      if (ret == NetErrorCode::kSEND_IO_FAIL) {
        ForcedClose(session_index, DisconnectReason::kSEND_ERROR);
      }
      return ret;
    }
#else  // !__USE_SEND_THREAD__

    if (send_mgr_.PostSendSession(session_index, session_uid, data_size,
                                  p_data) == false) {
      return NetErrorCode::kSEND_THREAD_BUFFER_FULL;
    }
#endif
    return NetErrorCode::kNONE;
  }

  bool ForcedClose(const std::int32_t session_index,
                   const std::uint64_t session_uid) {
    if (auto thread_index =
            session_mgr_.ForcedClose(session_index, session_uid);
        thread_index >= 0) {
      lazyed_dis_conn_mgr_.AddSession(thread_index, session_index, session_uid,
                                      DisconnectReason::kFORCES_CLOSE_SERVER);
      return true;
    }

    return false;
  }

 private:
  bool StartSendThread(NetConfig net_cfg) {
    return send_mgr_.Start(net_cfg.GetSendConfig());
  }

  void DisConnect(const std::int32_t session_index,
                  const std::uint64_t session_uid,
                  const DisconnectReason reason) {
    if (auto p_session = session_mgr_.GetSessionObj(session_index);
        p_session != nullptr) {
      DisConnect(p_session, session_uid, reason);
    }
  }

  void DisConnect(TcpSession* p_session, const std::uint64_t session_uid,
                  const DisconnectReason reason) {
    auto cur_time_sec = Helper::CurrentSecond();
    auto session_index = p_session->GetIndex();

    if (reason == DisconnectReason::kACCEPT_LOOP_REQUEST_QUEUE_ERROR ||
        reason == DisconnectReason::kACCEPT_LOOP_FAIL_ON_CONNECT) {
      p_session->DisConnectByAcceptFail();
      session_mgr_.ReturnSession(session_index, cur_time_sec);
      return;
    }

    if (p_session->Disconnect(session_uid)) {
      net_event_mgr_->OnClose(session_index, session_uid, reason);
      session_mgr_.ReturnSession(session_index, cur_time_sec);

#ifdef __USE_SEND_THREAD__
      send_mgr_.OnCloseSession(session_index);
#endif
    }
  }

  void ForcedClose(const std::int32_t session_index, DisconnectReason reason) {
    // 이 함수를 호출할 때는 절대 session_index는 유효한 값이다.
    auto p_session = session_mgr_.GetSessionObj(session_index);
    p_session->SetDisableNetwork();

    lazyed_dis_conn_mgr_.AddSession(p_session->GetRioThreadId(),
                                    p_session->GetIndex(), p_session->GetUID(),
                                    reason);
  }

  void SetDelegateDisConnect() {
    auto dis_connect_func = [&](TcpSession* p_session,
                                const uint64_t session_uid,
                                const DisconnectReason reason) {
      DisConnect(p_session, session_uid, reason);
    };
    io_worker_.DisConnectFunc = dis_connect_func;
    accepter_.DisConnectFunc = dis_connect_func;
    lazyed_dis_conn_mgr_.DisConnectFunc = dis_connect_func;

    auto dis_connect_func2 = [&](const int32_t session_index,
                                 const uint64_t session_uid,
                                 const DisconnectReason reason) {
      DisConnect(session_index, session_uid, reason);
    };
    send_mgr_.DisConnectFunc = dis_connect_func2;

    auto close_rioqueues_func = [&]() { CloseRIOQueues(); };
    io_worker_.CloseRIOQueues = close_rioqueues_func;
  }

  void CloseRIOQueues() {
    for (std::size_t i = 0; i < rio_completion_queues_.size(); ++i) {
      RIO.RIOCloseCompletionQueue(rio_completion_queues_[i]);
    }
  }

 private:
  NetConfig net_config_;

  INetEventManager* net_event_mgr_ = nullptr;

  ServerNetLazyedDisConnManager lazyed_dis_conn_mgr_;

  Accepter accepter_;

  ServerNetIOWorker io_worker_;

  std::vector<RIO_CQ> rio_completion_queues_;

  SOCKET listen_socket_ = INVALID_SOCKET;

  SessionManager session_mgr_;

  SendManager send_mgr_;

  UINT64 connect_sequence_id_ = 0;
};

}  // namespace rionetlite
