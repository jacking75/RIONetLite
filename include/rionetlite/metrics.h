#pragma once

#include <cstdint>

namespace rionetlite {

/// @api MetricsSnapshot - 서버와 event queue의 누적 계측 snapshot이다.
struct MetricsSnapshot final {
  std::uint64_t accepted_connections = 0;
  std::uint64_t active_connections = 0;
  std::uint64_t closed_connections = 0;
  std::uint64_t received_bytes = 0;
  std::uint64_t sent_bytes = 0;
  std::uint64_t send_would_block = 0;
  std::uint64_t stale_operations = 0;
  std::uint64_t protocol_errors = 0;
  std::uint64_t event_queue_high_watermark = 0;
  std::uint64_t event_queue_rejections = 0;
  std::uint64_t event_queue_depth = 0;
  std::uint64_t event_queue_dequeued = 0;
  std::uint64_t event_queue_wait_total_nanoseconds = 0;
  std::uint64_t event_queue_wait_max_nanoseconds = 0;
  std::uint64_t receive_operations = 0;
  std::uint64_t send_operations = 0;
  std::uint64_t rejected_connections = 0;
  std::uint64_t close_peer = 0;
  std::uint64_t close_application = 0;
  std::uint64_t close_error = 0;
  std::uint64_t corrupt_completion_queues = 0;
  std::uint64_t callback_count = 0;
  std::uint64_t callback_total_nanoseconds = 0;
  std::uint64_t callback_max_nanoseconds = 0;
};

}  // namespace rionetlite
