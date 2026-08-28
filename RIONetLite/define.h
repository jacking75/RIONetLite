#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <chrono>
#include <cstdint>

namespace rionetlite {
struct NetSendConfig {
  std::uint32_t max_session_count_ = 0;
  std::uint32_t max_worker_thread_count_ = 0;
  std::uint32_t max_send_packet_size_ = 0;
  std::uint32_t max_send_count_per_1sec_ =
      0;  // 정한 것 보다 조금 더 넉넉하게 주는 것을 추천
  std::uint32_t max_send_buffer_size_ = 0;
};

struct SessionNetConfig {
  std::int32_t max_recv_packet_size_ = 0;
  std::uint32_t max_try_1recv_buffer_size = 0;
  std::int32_t max_recv_count_per_1sec_ =
      0;  // 정한 것 보다 조금 더 넉넉하게 주는 것을 추천
  std::int32_t max_recv_buffer_size_ = 0;

  std::int32_t max_send_packet_size_ = 0;
  std::int32_t max_send_count_per_1sec_ =
      0;  // 0.2.x 호환 입력이다. 송신은 링 바이트로 배압을 판정한다.
  std::int32_t max_send_buffer_size_ = 0;

  std::uint32_t keepalive_time_ms_ = 0;      // 0이면 사용하지 않는다.
  std::uint32_t keepalive_interval_ms_ = 0;  // 0이면 사용하지 않는다.
};

struct NetConfig {
  std::uint16_t port_number_ = 0;
  std::int32_t io_work_thread_count_ = 0;
  std::int32_t io_send_thread_count_ =
      0;  // 0.2.x 호환 입력이다. 별도 send 스레드는 생성하지 않는다.

  std::int32_t max_session_count_ = 0;
  SessionNetConfig session_cfg_;
  std::uint32_t completion_batch_size_ = 64;
  std::uint32_t completion_queue_capacity_ = 0;
  std::uint32_t adaptive_spin_count_ = 128;
  std::uint32_t graceful_shutdown_timeout_ms_ = 5000;
  std::uint32_t peer_close_drain_timeout_ms_ = 5000;
  std::uint32_t idle_timeout_ms_ = 0;  // 0이면 사용하지 않는다.
  std::uint8_t worker_wait_mode_ = 1;
  bool enable_thread_affinity_ = false;

  NetSendConfig GetSendConfig() {
    NetSendConfig cfg;
    cfg.max_session_count_ = max_session_count_;
    cfg.max_worker_thread_count_ = io_send_thread_count_;
    cfg.max_send_packet_size_ = session_cfg_.max_send_packet_size_;
    cfg.max_send_count_per_1sec_ = session_cfg_.max_send_count_per_1sec_;
    cfg.max_send_buffer_size_ = session_cfg_.max_send_buffer_size_;
    return cfg;
  }
};

enum class DisconnectReason {
  kNONE,
  kACTIVE,
  kACCEPT_LOOP_REQUEST_QUEUE_ERROR,
  kACCEPT_LOOP_FAIL_ON_CONNECT,
  kZERO_COMPLETION,
  kIO_REQUEST_RECV_ERROR,
  kPARITAL_SEND_COMPLETION,
  kSEND_ERROR,
  kSEND_THREAD_REAL_SEND_ERROR,
  kFORCES_CLOSE_SERVER,
  kFORCES_CLOSE_SEND_ERROR,
  kRECV_BUFFER_FULL,
  kPROTOCOL_ERROR,
  kOVERLOADED,
  kIDLE_TIMEOUT,
  kEND
};
inline char DisconnectReasonStr[static_cast<std::int32_t>(
    DisconnectReason::kEND)][32] = {"none",
                                    "active",
                                    "accept_loop_request_queue_error",
                                    "accept_loop_fail_on_connect",
                                    "zero_completion",
                                    "io_request_recv_error",
                                    "partial_send_completion",
                                    "send_error",
                                    "send_thread_real_send_error",
                                    "forces_close_server",
                                    "forces_close_send_error",
                                    "recv_buffer_full",
                                    "protocol_error",
                                    "overloaded",
                                    "idle_timeout"};

// 0 ~ 100
enum class NetErrorCode : std::int16_t {
  kNONE = 0,
  kSESSION_COUNT_PER_THREAD_INCORRECT = 1,
  kWSASTARTUP_FAIL = 2,
  kLISTEN_SOCKET_CREATE_FAIL = 3,
  kLISTEN_SOCKET_BIND_FAIL = 5,
  kRIO_FUNCTION_INIT_FAIL = 6,
  kPREPARE_SESSION_POOL_FAIL = 7,
  kLISTEN_SOCKET_LISTEN_FAIL = 8,
  kSEND_INVALID_SESSION = 9,
  kSEND_IO_FAIL = 10,
  kSEND_SESSION_NETWORK_DISABLE = 11,
  kRECV_CLOSED_STATUS = 16,
  kRECV_DISABLE_NETWORK_STATUS = 17,
  kRECV_RIO_FAIL = 18,
  kSEND_THREAD_BUFFER_FULL = 19,
  kINVALID_CONFIG = 20,
  kIO_THREAD_START_FAIL = 21,
  kSEND_MESSAGE_TOO_LARGE = 22,
  kRECV_BUFFER_FULL = 23,
};

struct LazyedDisConnectSession {
  std::int32_t session_index_ = -1;
  std::uint64_t uid = 0;
  DisconnectReason reason_ = DisconnectReason::kNONE;
  // peer FIN 이후 송신 드레인을 기다리는 항목의 마감 tick이다. 0이면 아직
  // 보류된 적이 없다는 뜻이다.
  std::uint64_t drain_deadline_ms_ = 0;
};

}  // namespace rionetlite
