#pragma once

#include "../../src/detail/win32_resources.h"
#include "../define.h"
#include "fast_spin_lock.h"
#include "ring_buffer.h"
#include "rio_context.h"
#include "send_immediately.h"

#include <cstdint>
#include <format>
#include <utility>
#include <vector>

namespace rionetlite {
class TcpSession {
  const std::int64_t kCONNECTED = 1;
  const std::int64_t kENABLE_VALUE = 1;

 public:
  TcpSession() = default;
  ~TcpSession() = default;

  int GetIndex() { return index_; }

  std::uint64_t GetUID() { return uid_; }

  RIO_RQ GetRIORQ() { return rio_rq_; }

  bool IsEnableNetwork() { return enable_network_ == kENABLE_VALUE; }

  void SetDisableNetwork() { enable_network_ = 0; }

  void Init(const int index, const int threadId) {
    rio_thread_id_ = threadId;
    index_ = index;
    memset(&client_addr_, 0, sizeof(SOCKADDR_IN));
  }

  bool RioInitialize(const SessionNetConfig net_config) {
    max_try_1recv_buffer_size_ = net_config.max_try_1recv_buffer_size;

    if (io_sender_.Init(index_, net_config) == false) {
      LogFuncPtr((int)LogLevel::kERROR,
                 "TcpSession::RioInitialize() - fail io_sender_.Init");
      return false;
    }

    if (auto [err_code, err_msg] =
            recv_buffer_.Create(false, net_config.max_recv_buffer_size_);
        err_code != (int)NetErrorCode::kNONE) {
      LogFuncPtr(
          (int)LogLevel::kERROR,
          std::format(
              "TcpSession::RioInitialize() - fail mRecvBuffer.Create. {0}, {1}",
              err_code, err_msg)
              .c_str());
      return false;
    }

    if (!recv_rio_buffer_.Register(RIO, recv_buffer_.GetBuffer(),
                                   recv_buffer_.EnableBufferSize())) {
      LogFuncPtr(
          (int)LogLevel::kERROR,
          std::format(
              "TcpSession::RioInitialize() - RIORegisterBuffer Error: {0}",
              GetLastError())
              .c_str());

      return false;
    }

    recv_context_.Init(index_, IOType::kRECV);
    return true;
  }

  void Dispose() {
    static_cast<void>(recv_rio_buffer_.Reset());

    io_sender_.Dispose();
  }

  bool OnConnect(const std::uint64_t uid, detail::UniqueSocket socket,
                 RIO_RQ rio_rq) {
    // 세션 객체가 재 사용되더라도 대기 시간이 있으므로 연결이 끊어질 때와 이
    // 함수 호출이 겹쳐지지 않는다.
    SetAddress(socket.get());

    /// turn off nagle
    int opt = 1;
    setsockopt(socket.get(), IPPROTO_TCP, TCP_NODELAY, (const char*)&opt,
               sizeof(int));

    socket_ = std::move(socket);
    uid_ = uid;
    connected_ = kCONNECTED;
    enable_network_ = kENABLE_VALUE;
    rio_rq_ = rio_rq;
    io_sender_.OnConnect(rio_rq_);
    recv_buffer_.Clear();

    return PostRecv() == NetErrorCode::kNONE;
  }

  bool IsConnected() const { return connected_ == kCONNECTED; }

  int GetRioThreadId() const { return rio_thread_id_; }

  bool PostSend(const UINT32 data_size, const char* p_data) {
    if (IsConnected() == false) {
      return false;
    }

    FastSpinlockGuard criticalSection(send_lock_);
    auto result = io_sender_.PostSend(GetUID(), data_size, p_data);
    return result;
  }

  void SendCompletion(RioIoContext* p_send_context) {
    if (IsConnected() == false) {
      return;
    }

    FastSpinlockGuard criticalSection(send_lock_);
    io_sender_.Completed(p_send_context);
  }

  NetErrorCode PostRecv() {
    // FastSpinlockGuard criticalSection(session_lock_);

    if (IsConnected() == false) {
      return NetErrorCode::kRECV_CLOSED_STATUS;
    }

    if (IsEnableNetwork() == false) {
      return NetErrorCode::kRECV_DISABLE_NETWORK_STATUS;
    }

    recv_context_.session_unique_Id_ = GetUID();
    recv_context_.BufferId = recv_rio_buffer_.get();
    recv_context_.Length = max_try_1recv_buffer_size_;
    recv_context_.Offset = recv_buffer_.GetWriteablePos();

    /// start async recv
    DWORD flags = 0;
    if (!RIO.RIOReceive(rio_rq_, (PRIO_BUF)&recv_context_, 1, flags,
                        &recv_context_)) {
      WriteLog_FailPostRecv();
      return NetErrorCode::kRECV_RIO_FAIL;
    }

    return NetErrorCode::kNONE;
  }

  void RecvCompletion(const std::uint32_t size) { recv_buffer_.Write(size); }

  std::tuple<std::uint32_t, char*> ReadRecvBuffer() {
    return recv_buffer_.Read();
  }

  void IncremantReadPosRecvBuffer(const std::uint32_t size) {
    recv_buffer_.IncremantReadPos(size);
  }

  bool Disconnect(const std::uint64_t uid) {
    if (IsConnected() == false) {
      return false;
    }

    if (GetUID() != uid) {
      return false;
    }

    // FastSpinlockGuard criticalSection(session_lock_);

    LINGER lingerOption;
    lingerOption.l_onoff = 1;
    lingerOption.l_linger = 0;

    /// no TCP TIME_WAIT
    setsockopt(socket_.get(), SOL_SOCKET, SO_LINGER, (char*)&lingerOption,
               sizeof(LINGER));

    socket_.Reset();
    connected_ = 0;
    SetDisableNetwork();
    uid_ = 0;

    return true;
  }

  void DisConnectByAcceptFail() {
    socket_.Reset();
    connected_ = 0;
    SetDisableNetwork();
    uid_ = 0;
  }

 private:
  void SetAddress(SOCKET socket) {
    SOCKADDR_IN clientaddr;
    int addrlen = sizeof(clientaddr);
    getpeername(socket, (SOCKADDR*)&clientaddr, &addrlen);
    memcpy(&client_addr_, &clientaddr, sizeof(SOCKADDR_IN));
  }

  void WriteLog_FailPostRecv() {
    char logmsg[256] = {
        0,
    };
    sprintf_s(logmsg, "TcpSession::PostRecv() - RIOReceive Error: %d",
              GetLastError());
    LogFuncPtr((int)LogLevel::kERROR, logmsg);
  }

  FastSpinlock send_lock_;

  int index_ = -1;
  std::uint32_t max_try_1recv_buffer_size_ = 0;
  int rio_thread_id_ = -1;

  std::uint64_t uid_ = 0;

  // 64비트에서 INT64는 아토믹하게 읽기,쓰기 가능. ++i 가 아닌 i = 1 등이
  // 아토믹하다는 것이다.
  // https://docs.microsoft.com/en-us/windows/win32/sync/interlocked-variable-access
  std::int64_t connected_ = 0;

  std::int64_t enable_network_ = 0;

  detail::UniqueSocket socket_;
  SOCKADDR_IN client_addr_;

  RIO_RQ rio_rq_ = NULL;
  RioIoContext recv_context_;

  SendImmediately io_sender_;

  RingBufferVA2 recv_buffer_;
  detail::RegisteredRioBuffer recv_rio_buffer_;
};

}  // namespace rionetlite
