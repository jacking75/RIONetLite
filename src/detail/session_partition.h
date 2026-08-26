#pragma once

#include <cstdint>

namespace rionetlite::detail {

struct SessionPartition final {
  std::uint32_t first_index = 0;
  std::uint32_t count = 0;
};

constexpr SessionPartition PartitionSessions(
    const std::uint32_t max_sessions, const std::uint32_t worker_count,
    const std::uint32_t worker_index) noexcept {
  if (worker_count == 0 || worker_index >= worker_count) {
    return {};
  }
  const std::uint32_t base = max_sessions / worker_count;
  const std::uint32_t remainder = max_sessions % worker_count;
  const std::uint32_t extra_before =
      worker_index < remainder ? worker_index : remainder;
  return SessionPartition{
      .first_index = worker_index * base + extra_before,
      .count = base + (worker_index < remainder ? 1U : 0U),
  };
}

}  // namespace rionetlite::detail
