#pragma once

#include "../define.h"
#include "fast_spin_lock.h"
#include "rio_context.h"
#include "send_ring_buffer.h"

#include <cstdint>
#include <format>

namespace rionetlite {
// 보낼 데이터를 모아서 주기적으로 한번에 보내기
class SendSession {
 public:
  SendSession() = default;
  ~SendSession() = default;

  std::int32_t GetIndex() { return index_; }
  std::uint64_t GetUID() { return uid_; }

  char* GetBuffer() { return send_buffer_.GetBuffer(); }

  bool IsConnected() { return is_connected_; }

  bool Init(const std::int32_t session_index, NetSendConfig net_cfg) {
    index_ = session_index;
    max_send_packet_size_ = net_cfg.max_send_packet_size_;
    max_send_count_per_1sec_ = net_cfg.max_send_count_per_1sec_;
    max_send_ring_buffer_size_ = net_cfg.max_send_buffer_size_;

    send_buffer_.Create(max_send_ring_buffer_size_);

    sendContext_.Init(index_, IOType::kSEND);

    send_rio_buffer_id_ = RIO.RIORegisterBuffer(
        send_buffer_.GetBuffer(), send_buffer_.EnableBufferSize());
    if (send_rio_buffer_id_ == RIO_INVALID_BUFFERID) {
      LogFuncPtr(
          (int)LogLevel::kERROR,
          std::format("SendSession::Init() - RIORegisterBuffer Error: {0}",
                      GetLastError())
              .c_str());
      return false;
    }

    Clear();

    return true;
  }

  void Dispose() { RIO.RIODeregisterBuffer(send_rio_buffer_id_); }

  void OnConnect(const std::uint64_t uid, RIO_RQ request_queue) {
    is_connected_ = true;
    uid_ = uid;
    ref_rio_rq_ = request_queue;
    Clear();
  }

  void OnDisConnect() { is_connected_ = false; }

  bool PostSend(const std::uint32_t data_size, const char* p_data) {
    FastSpinlockGuard criticalSection(session_lock_);

    if (send_buffer_.Write(data_size, p_data) == false) {
      return false;
    }

    return true;
  }

  void SendCompleted(const std::uint32_t length) {
    FastSpinlockGuard criticalSection(session_lock_);

    send_buffer_.IncremantReadPos(length);
    send_buffer_.MoveWrittenDataForward();
  }

  bool RealSend() {
    FastSpinlockGuard criticalSection(session_lock_);

    auto send_size = send_buffer_.SendAbleSize();

    if (send_size <= 0) {
      return true;
    }

    if (send_size > (uint32_t)max_send_packet_size_) {
      send_size = max_send_packet_size_;
    }

    sendContext_.session_index_ = index_;
    sendContext_.BufferId = send_rio_buffer_id_;
    sendContext_.Length = send_size;
    sendContext_.Offset = send_buffer_.GetReadPos();

    /// start async send
    DWORD flags = 0;
    if (!RIO.RIOSend(ref_rio_rq_, (PRIO_BUF)&sendContext_, 1, flags,
                     &sendContext_)) {
      LogFuncPtr((int)LogLevel::kERROR,
                 std::format("Session::RealSend() - RIOSend Error: {0}",
                             GetLastError())
                     .c_str());
      return false;
    }

    return true;
  }

 private:
  void Clear() { send_buffer_.Clear(); }

  std::int32_t index_ = 0;
  std::uint64_t uid_ = 0;
  std::int32_t max_send_packet_size_ = 0;
  std::uint64_t max_send_count_per_1sec_ = 0;
  std::int32_t max_send_ring_buffer_size_ = 0;
  RioIoContext sendContext_;
  RIO_BUFFERID send_rio_buffer_id_ = NULL;

  bool is_connected_ = false;
  RIO_RQ ref_rio_rq_ = NULL;

  FastSpinlock session_lock_;
  SendRingBuffer send_buffer_;
};

}  // namespace rionetlite