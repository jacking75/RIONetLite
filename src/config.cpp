#include "rionetlite/config.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

namespace rionetlite {
namespace {

std::unexpected<Error> Invalid(std::string field, std::string message) {
  return std::unexpected(Error{
      .category = ErrorCategory::kConfiguration,
      .code = ErrorCode::kInvalidConfiguration,
      .native_code = 0,
      .field = std::move(field),
      .message = std::move(message),
  });
}

bool IsPageAligned(const std::uint32_t value,
                   const std::uint32_t page_size) noexcept {
  return value != 0 && value % page_size == 0;
}

}  // namespace

std::uint32_t DefaultIoWorkerCount() noexcept {
  const DWORD processors = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
  if (processors == 0) {
    return 1;
  }
  return std::clamp<std::uint32_t>(processors > 2 ? processors - 1 : processors,
                                   1, 256);
}

std::expected<ValidatedServerConfig, Error> ValidateServerConfig(
    const ServerConfig& config) {
  SYSTEM_INFO system_info{};
  GetSystemInfo(&system_info);
  const auto page_size = system_info.dwPageSize;

  if (config.port == 0) {
    return Invalid("port", "port must be in the range 1..65535");
  }
  if (config.io_worker_count == 0) {
    return Invalid("io_worker_count", "io_worker_count must be at least 1");
  }
  constexpr std::uint32_t kMaxIoWorkerCount = 256;
  if (config.io_worker_count > kMaxIoWorkerCount) {
    return Invalid("io_worker_count", "io_worker_count must not exceed 256");
  }
  if (config.max_sessions == 0) {
    return Invalid("max_sessions", "max_sessions must be at least 1");
  }
  if (config.max_sessions >
      static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
    return Invalid("max_sessions",
                   "max_sessions exceeds the current implementation limit");
  }
  if (config.io_worker_count > config.max_sessions) {
    return Invalid("io_worker_count",
                   "io_worker_count cannot exceed max_sessions");
  }
  if (!IsPageAligned(config.receive_buffer_bytes, page_size)) {
    return Invalid("receive_buffer_bytes",
                   "receive_buffer_bytes must be a non-zero multiple of the "
                   "Windows page size");
  }
  constexpr std::uint32_t kMaxMappedBufferBytes = 1024U * 1024U * 1024U;
  if (config.receive_buffer_bytes > kMaxMappedBufferBytes ||
      config.send_buffer_bytes > kMaxMappedBufferBytes) {
    return Invalid("buffer_bytes",
                   "registered buffers cannot exceed 1 GiB per direction");
  }
  if (!IsPageAligned(config.send_buffer_bytes, page_size)) {
    return Invalid("send_buffer_bytes",
                   "send_buffer_bytes must be a non-zero multiple of the "
                   "Windows page size");
  }
  if (config.max_receive_chunk_bytes == 0 ||
      config.max_receive_chunk_bytes > config.receive_buffer_bytes) {
    return Invalid("max_receive_chunk_bytes",
                   "max_receive_chunk_bytes must be in the range "
                   "1..receive_buffer_bytes");
  }
  if (config.max_message_bytes == 0 ||
      config.max_message_bytes > config.receive_buffer_bytes ||
      config.max_message_bytes > config.send_buffer_bytes) {
    return Invalid("max_message_bytes",
                   "max_message_bytes must fit in both receive and send "
                   "buffers");
  }
  if (config.max_outstanding_sends_per_session == 0) {
    return Invalid("max_outstanding_sends_per_session",
                   "max_outstanding_sends_per_session must be at least 1");
  }
  const std::uint64_t required_send_bytes =
      static_cast<std::uint64_t>(config.max_message_bytes) *
      config.max_outstanding_sends_per_session;
  if (required_send_bytes > config.send_buffer_bytes) {
    return Invalid("send_buffer_bytes",
                   "send_buffer_bytes must hold one maximum message for each "
                   "outstanding send");
  }
  if (config.completion_batch_size == 0 ||
      config.completion_batch_size > 4096) {
    return Invalid("completion_batch_size",
                   "completion_batch_size must be in the range 1..4096");
  }
  if (config.adaptive_spin_count > 1'000'000) {
    return Invalid("adaptive_spin_count",
                   "adaptive_spin_count must be in the range 0..1000000");
  }
  if (config.graceful_shutdown_timeout_ms == 0 ||
      config.graceful_shutdown_timeout_ms > 300'000) {
    return Invalid("graceful_shutdown_timeout_ms",
                   "graceful_shutdown_timeout_ms must be in the range "
                   "1..300000");
  }

  const std::uint64_t sessions_per_worker =
      (static_cast<std::uint64_t>(config.max_sessions) +
       config.io_worker_count - 1) /
      config.io_worker_count;
  const std::uint64_t completion_capacity =
      sessions_per_worker * (1ULL + config.max_outstanding_sends_per_session);
  if (completion_capacity > std::numeric_limits<ULONG>::max()) {
    return Invalid("max_outstanding_sends_per_session",
                   "calculated RIO completion queue capacity exceeds ULONG");
  }

  const std::uint64_t registered_bytes_per_session =
      static_cast<std::uint64_t>(config.receive_buffer_bytes) +
      config.send_buffer_bytes;
  const std::uint64_t reserved_bytes_per_session =
      2ULL * registered_bytes_per_session;
  constexpr std::uint64_t kEstimatedSessionControlBytes = 512;
  constexpr std::uint64_t kEstimatedSendContextBytes = 64;
  const std::uint64_t control_bytes_per_session =
      kEstimatedSessionControlBytes +
      kEstimatedSendContextBytes * config.max_outstanding_sends_per_session;
  if (config.max_sessions >
      std::numeric_limits<std::uint64_t>::max() / reserved_bytes_per_session) {
    return Invalid("max_sessions",
                   "calculated registered memory size overflows uint64");
  }
  if (config.max_sessions >
      std::numeric_limits<std::uint64_t>::max() / control_bytes_per_session) {
    return Invalid("max_sessions",
                   "calculated session control memory overflows uint64");
  }
  const std::uint64_t session_control_bytes =
      control_bytes_per_session * config.max_sessions;
  constexpr std::uint64_t kEstimatedWorkerMetricBytes = 256;
  const std::uint64_t worker_result_bytes =
      static_cast<std::uint64_t>(config.io_worker_count) *
      (config.completion_batch_size * 32ULL + kEstimatedWorkerMetricBytes);
  if (session_control_bytes >
      std::numeric_limits<std::uint64_t>::max() - worker_result_bytes) {
    return Invalid("io_worker_count",
                   "calculated total control memory overflows uint64");
  }

  return ValidatedServerConfig{
      .value = config,
      .max_sessions_per_worker =
          static_cast<std::uint32_t>(sessions_per_worker),
      .completion_queue_capacity =
          static_cast<std::uint32_t>(completion_capacity),
      .estimated_registered_bytes =
          reserved_bytes_per_session * config.max_sessions,
      .estimated_reserved_virtual_bytes =
          reserved_bytes_per_session * config.max_sessions,
      .estimated_committed_bytes =
          registered_bytes_per_session * config.max_sessions,
      .estimated_control_bytes = session_control_bytes + worker_result_bytes,
  };
}

}  // namespace rionetlite
