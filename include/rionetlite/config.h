#pragma once

#include "rionetlite/error.h"

#include <cstdint>
#include <expected>

namespace rionetlite {

struct ServerConfig final {
  std::uint16_t port = 0;
  std::uint32_t io_worker_count = 1;
  std::uint32_t max_sessions = 1024;
  std::uint32_t max_receive_chunk_bytes = 4096;
  std::uint32_t receive_buffer_bytes = 64 * 1024;
  std::uint32_t max_message_bytes = 16 * 1024;
  std::uint32_t send_buffer_bytes = 64 * 1024;
  std::uint32_t max_outstanding_sends_per_session = 16;
  std::uint32_t completion_batch_size = 64;
};

struct ValidatedServerConfig final {
  ServerConfig value;
  std::uint32_t max_sessions_per_worker = 0;
  std::uint32_t completion_queue_capacity = 0;
  std::uint64_t estimated_registered_bytes = 0;
};

std::expected<ValidatedServerConfig, Error> ValidateServerConfig(
    const ServerConfig& config);

}  // namespace rionetlite
