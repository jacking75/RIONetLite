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
  std::uint32_t send_buffer_bytes = 256 * 1024;
  std::uint32_t max_outstanding_sends_per_session = 16;
  std::uint32_t completion_batch_size = 64;
  WorkerWaitMode worker_wait_mode = WorkerWaitMode::kAdaptive;
  std::uint32_t adaptive_spin_count = 128;
  bool enable_thread_affinity = false;
  bool enable_metrics = true;
  std::uint32_t graceful_shutdown_timeout_ms = 5000;
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
