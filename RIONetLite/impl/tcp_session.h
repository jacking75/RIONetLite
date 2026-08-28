#pragma once

#include "../../src/detail/rio_api.h"
#include "../../src/detail/session_lifecycle.h"
#include "../../src/detail/win32_resources.h"
#include "../define.h"
#include "fast_spin_lock.h"
#include "ring_buffer.h"
#include "rio_context.h"
#include "send_immediately.h"

#include <mstcpip.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <tuple>
#include <utility>

namespace rionetlite {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif

class alignas(64) TcpSession {
 public:
  TcpSession(const std::uint32_t index, const std::uint32_t thread_id)
      : lifecycle_(index),
        index_(static_cast<int>(index)),
        rio_thread_id_(static_cast<int>(thread_id)) {}
  ~TcpSession() = default;

  int GetIndex() { return index_; }

  std::uint64_t GetUID() const { return lifecycle_.id().generation; }

  RIO_RQ GetRIORQ() { return rio_rq_; }

  bool IsEnableNetwork() const {
    return enable_network_.load(std::memory_order_acquire);
  }

  void SetDisableNetwork() {
    enable_network_.store(false, std::memory_order_release);
    static_cast<void>(lifecycle_.TryBeginClose());
    io_sender_.BeginClose();
  }

  bool TryReserve() noexcept { return lifecycle_.TryReserve(); }

  bool Matches(const std::uint64_t generation) const noexcept {
    return lifecycle_.Matches(SessionId{
        .index = static_cast<std::uint32_t>(index_),
        .generation = generation,
    });
  }

  bool RioInitialize(const SessionNetConfig net_config,
                     detail::RioApi& rio_api) {
    rio_api_ = &rio_api;
    max_try_1recv_buffer_size_ = net_config.max_try_1recv_buffer_size;
    keepalive_time_ms_ = net_config.keepalive_time_ms_;
    keepalive_interval_ms_ = net_config.keepalive_interval_ms_;

    if (io_sender_.Init(index_, net_config, rio_api,
                        static_cast<std::uint32_t>(rio_thread_id_)) == false) {
      return false;
    }

    if (const auto create_result =
            recv_buffer_.Create(net_config.max_recv_buffer_size_);
        std::get<0>(create_result) != static_cast<int>(NetErrorCode::kNONE)) {
      return false;
    }

    if (!recv_rio_buffer_.Register(rio_api, recv_buffer_.GetBuffer(),
                                   recv_buffer_.RegisteredSpanSize())) {
      return false;
    }

    recv_context_.Init(index_, IOType::kRECV,
                       static_cast<std::uint32_t>(rio_thread_id_));
    return true;
  }

  void Dispose() {
    static_cast<void>(recv_rio_buffer_.Reset());

    io_sender_.Dispose();
  }

  bool OnConnect(const std::uint64_t uid, detail::UniqueSocket socket,
                 RIO_RQ rio_rq) {
    if (!SetAddress(socket.get())) {
      return false;
    }

    int opt = 1;
    if (setsockopt(socket.get(), IPPROTO_TCP, TCP_NODELAY,
                   reinterpret_cast<const char*>(&opt),
                   sizeof(opt)) == SOCKET_ERROR) {
      return false;
    }

    // keepalive는 무통신 상태로 사라진 피어의 미완료 수신을 오류로 완료시켜
    // 기존 오류 경로가 세션 슬롯을 회수하게 한다.
    if (keepalive_time_ms_ != 0 && keepalive_interval_ms_ != 0) {
      tcp_keepalive keepalive{};
      keepalive.onoff = 1;
      keepalive.keepalivetime = keepalive_time_ms_;
      keepalive.keepaliveinterval = keepalive_interval_ms_;
      DWORD returned = 0;
      static_cast<void>(WSAIoctl(socket.get(), SIO_KEEPALIVE_VALS, &keepalive,
                                 sizeof(keepalive), nullptr, 0, &returned,
                                 nullptr, nullptr));
    }

    socket_ = std::move(socket);
    rio_rq_ = rio_rq;
    io_sender_.OnConnect(rio_rq_, uid);
    recv_buffer_.Clear();
    TouchActivity();
    enable_network_.store(true, std::memory_order_release);
    accepting_application_sends_.store(true, std::memory_order_release);
    socket_closed_.store(false, std::memory_order_release);
    if (!Matches(uid) || !lifecycle_.TryActivate()) {
      return false;
    }

    return true;
  }

  bool IsConnected() const {
    return lifecycle_.state() == detail::SessionState::kActive;
  }

  bool HasOpenSocket() const noexcept {
    return !socket_closed_.load(std::memory_order_acquire);
  }

  int GetRioThreadId() const { return rio_thread_id_; }

  NetErrorCode PostSend(const std::uint64_t expected_uid,
                        const UINT32 data_size, const char* p_data) {
    if (IsConnected() == false ||
        !accepting_application_sends_.load(std::memory_order_acquire)) {
      return NetErrorCode::kSEND_SESSION_NETWORK_DISABLE;
    }

    return io_sender_.PostSend(expected_uid, data_size, p_data);
  }

  NetErrorCode PostSend(const std::uint64_t expected_uid,
                        const std::uint32_t header_size, const char* p_header,
                        const std::uint32_t payload_size,
                        const char* p_payload) {
    if (IsConnected() == false ||
        !accepting_application_sends_.load(std::memory_order_acquire)) {
      return NetErrorCode::kSEND_SESSION_NETWORK_DISABLE;
    }

    return io_sender_.PostSend(expected_uid, header_size, p_header,
                               payload_size, p_payload);
  }

  bool SendCompletion(RioIoContext* p_send_context,
                      const std::uint32_t transferred) {
    if (IsConnected() == false) {
      io_sender_.Failed(p_send_context);
      return false;
    }

    return io_sender_.Completed(p_send_context, transferred);
  }

  void SendFailed(RioIoContext* p_send_context) {
    io_sender_.Failed(p_send_context);
  }

  void RecvFailed() {
    recv_context_.Fail();
    recv_rio_buffer_.CompleteOutstanding();
  }

  std::uint32_t Outstanding() const noexcept {
    return recv_rio_buffer_.outstanding() + io_sender_.Outstanding();
  }

  std::uint32_t SendOutstanding() const noexcept {
    return io_sender_.Outstanding();
  }

  void BeginPeerClose() noexcept {
    accepting_application_sends_.store(false, std::memory_order_release);
  }

  bool ReadyForPeerClose() const noexcept { return SendOutstanding() == 0; }

  // 종료 타임아웃 경로에서 커널이 미완료 요청을 취소하도록 소켓만 닫는다.
  void AbortSocket() noexcept {
    if (!socket_) {
      return;
    }
    socket_closed_.store(true, std::memory_order_release);
    enable_network_.store(false, std::memory_order_release);
    accepting_application_sends_.store(false, std::memory_order_release);
    socket_.Reset();
  }

  // 드레인에 실패했을 때만 사용한다. 등록 버퍼와 매핑을 해제하지 않고 소유권만
  // 포기해 커널이 참조 중인 메모리를 회수하지 않는다.
  void LeakRegisteredResources() noexcept {
    recv_rio_buffer_.Release();
    recv_buffer_.ReleaseStorage();
    io_sender_.LeakRegisteredResources();
  }

  void TouchActivity() noexcept {
    last_activity_ms_.store(GetTickCount64(), std::memory_order_relaxed);
  }

  [[nodiscard]] bool IsIdleFor(const std::uint64_t now_ms,
                               const std::uint64_t timeout_ms) const noexcept {
    const auto last = last_activity_ms_.load(std::memory_order_relaxed);
    return now_ms > last && now_ms - last >= timeout_ms;
  }

  NetErrorCode PostRecv() {
    // FastSpinlockGuard criticalSection(session_lock_);

    if (IsConnected() == false) {
      return NetErrorCode::kRECV_CLOSED_STATUS;
    }

    if (IsEnableNetwork() == false) {
      return NetErrorCode::kRECV_DISABLE_NETWORK_STATUS;
    }

    const auto writable = recv_buffer_.GetWritableSize();
    if (writable == 0) {
      return NetErrorCode::kRECV_BUFFER_FULL;
    }
    const auto receive_length = std::min(max_try_1recv_buffer_size_, writable);
    if (!recv_context_.BeginRequest(
            GetUID(), recv_rio_buffer_.get(), recv_buffer_.GetWriteablePos(),
            receive_length, recv_buffer_.RegisteredSpanSize())) {
      return NetErrorCode::kRECV_RIO_FAIL;
    }

    /// start async recv
    DWORD flags = 0;
    recv_rio_buffer_.AddOutstanding();
    if (!rio_api_->Receive(rio_rq_, reinterpret_cast<PRIO_BUF>(&recv_context_),
                           1, flags, &recv_context_)) {
      recv_context_.Fail();
      recv_rio_buffer_.CompleteOutstanding();
      return NetErrorCode::kRECV_RIO_FAIL;
    }

    return NetErrorCode::kNONE;
  }

  bool RecvCompletion(const std::uint32_t size) {
    recv_rio_buffer_.CompleteOutstanding();
    if (!recv_context_.CompleteReceive(size) || !recv_buffer_.Write(size)) {
      recv_context_.Fail();
      return false;
    }
    TouchActivity();
    return true;
  }

  std::tuple<std::uint32_t, char*> ReadRecvBuffer() {
    return recv_buffer_.Read();
  }

  bool ConsumeRecvBuffer(const std::uint32_t size) {
    const auto [readable, data] = recv_buffer_.Read();
    static_cast<void>(data);
    if (size > readable) {
      return false;
    }
    recv_buffer_.IncremantReadPos(size);
    return true;
  }

  bool Disconnect(const std::uint64_t uid, const bool abortive = true) {
    if (!Matches(uid)) {
      return false;
    }

    static_cast<void>(lifecycle_.TryBeginClose());
    if (lifecycle_.state() != detail::SessionState::kDraining) {
      return false;
    }

    bool expected = false;
    if (!socket_closed_.compare_exchange_strong(expected, true,
                                                std::memory_order_acq_rel)) {
      return false;
    }

    io_sender_.BeginClose();

    if (abortive) {
      LINGER linger_option{};
      linger_option.l_onoff = 1;
      linger_option.l_linger = 0;
      setsockopt(socket_.get(), SOL_SOCKET, SO_LINGER,
                 reinterpret_cast<const char*>(&linger_option),
                 sizeof(linger_option));
    } else {
      static_cast<void>(shutdown(socket_.get(), SD_SEND));
    }

    socket_.Reset();
    enable_network_.store(false, std::memory_order_release);
    accepting_application_sends_.store(false, std::memory_order_release);
    static_cast<void>(lifecycle_.TryMarkClosing());
    const bool notify_close = lifecycle_.TryNotifyClose();
    static_cast<void>(lifecycle_.TryQuarantine());

    return notify_close;
  }

  void DisConnectByAcceptFail() {
    socket_.Reset();
    enable_network_.store(false, std::memory_order_release);
    accepting_application_sends_.store(false, std::memory_order_release);
    socket_closed_.store(true, std::memory_order_release);
    static_cast<void>(lifecycle_.TryBeginClose());
    static_cast<void>(lifecycle_.TryMarkClosing());
    static_cast<void>(lifecycle_.TryNotifyClose());
    static_cast<void>(lifecycle_.TryQuarantine());
  }

  bool TryRecycle() noexcept { return lifecycle_.TryRecycle(Outstanding(), 0); }

 private:
  bool SetAddress(SOCKET socket) {
    SOCKADDR_IN clientaddr{};
    int addrlen = sizeof(clientaddr);
    if (getpeername(socket, reinterpret_cast<SOCKADDR*>(&clientaddr),
                    &addrlen) == SOCKET_ERROR) {
      return false;
    }
    client_addr_ = clientaddr;
    return true;
  }

  int index_ = -1;
  std::uint32_t max_try_1recv_buffer_size_ = 0;
  std::uint32_t keepalive_time_ms_ = 0;
  std::uint32_t keepalive_interval_ms_ = 0;
  int rio_thread_id_ = -1;

  detail::SessionLifecycle lifecycle_;
  std::atomic<bool> enable_network_{false};
  std::atomic<bool> accepting_application_sends_{false};
  std::atomic<bool> socket_closed_{true};
  std::atomic<std::uint64_t> last_activity_ms_{0};

  detail::UniqueSocket socket_;
  SOCKADDR_IN client_addr_{};

  RIO_RQ rio_rq_ = RIO_INVALID_RQ;
  RioIoContext recv_context_{};

  SendImmediately io_sender_;

  RingBufferVA2 recv_buffer_;
  detail::RegisteredRioBuffer recv_rio_buffer_;
  detail::RioApi* rio_api_ = nullptr;
};

static_assert(alignof(TcpSession) == 64);

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}  // namespace rionetlite
