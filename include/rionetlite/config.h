#pragma once

#include "rionetlite/error.h"

#include <cstdint>
#include <expected>

namespace rionetlite {

/// @api WorkerWaitMode - RIO worker의 대기 전략이다.
enum class WorkerWaitMode : std::uint8_t {
  kPolling,
  kAdaptive,
  kNotification,
};

/// @api DefaultIoWorkerCount - processor group을 반영한 기본 worker 수다.
std::uint32_t DefaultIoWorkerCount() noexcept;

/// @api ServerConfig - 서버 생성 전 사용자가 지정하는 설정이다.
struct ServerConfig final {
  std::uint16_t port = 0;
  std::uint32_t io_worker_count = DefaultIoWorkerCount();
  std::uint32_t max_sessions = 1024;
  std::uint32_t max_receive_chunk_bytes = 4096;
  std::uint32_t receive_buffer_bytes = 64 * 1024;
  std::uint32_t max_message_bytes = 16 * 1024;
  // 세션당 송신 링 크기다. 송신 배압은 in-flight 요청 수가 아니라 이 링에 남은
  // 바이트로 판정하므로, 여기에 들어가지 못한 메시지만 kWouldBlock이 된다.
  std::uint32_t send_buffer_bytes = 256 * 1024;
  std::uint32_t completion_batch_size = 64;
  WorkerWaitMode worker_wait_mode = WorkerWaitMode::kAdaptive;
  std::uint32_t adaptive_spin_count = 128;
  bool enable_thread_affinity = false;
  bool enable_metrics = true;
  // 콜백 소요 시간 계측을 끄면 수신·송신마다 QPC 2회를 절약한다.
  bool enable_callback_timing = true;
  std::uint32_t graceful_shutdown_timeout_ms = 5000;
  // peer FIN 이후 미완료 송신을 기다리는 상한이다. 초과하면 abortive close로
  // 강등해 세션 슬롯을 회수한다.
  std::uint32_t peer_close_drain_timeout_ms = 5000;
  // 0이면 사용하지 않는다. 지정하면 keepalive를 켜고, 무통신 세션을 worker가
  // 약 1초 주기로 훑어 kIdleTimeout으로 닫는다.
  std::uint32_t idle_timeout_ms = 0;
  std::uint32_t keepalive_time_ms = 0;
  std::uint32_t keepalive_interval_ms = 1000;
};

/// @api ValidatedServerConfig - 검증된 설정과 계산된 자원 예산이다.
struct ValidatedServerConfig final {
  ServerConfig value;
  std::uint32_t max_sessions_per_worker = 0;
  std::uint32_t completion_queue_capacity = 0;
  std::uint64_t estimated_registered_bytes = 0;
  std::uint64_t estimated_reserved_virtual_bytes = 0;
  std::uint64_t estimated_committed_bytes = 0;
  std::uint64_t estimated_control_bytes = 0;
};

/// @api ValidateServerConfig - 설정 범위와 메모리/CQ 관계를 검증한다.
std::expected<ValidatedServerConfig, Error> ValidateServerConfig(
    const ServerConfig& config);

}  // namespace rionetlite
