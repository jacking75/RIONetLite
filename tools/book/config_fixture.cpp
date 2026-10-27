#include "rionetlite/config.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
struct Case {
  std::string_view id;
  rionetlite::ServerConfig config;
};

void WriteConfig(const rionetlite::ServerConfig& c, const DWORD page_size) {
  std::cout
      << "{\"port\":" << c.port << ",\"io_worker_count\":" << c.io_worker_count
      << ",\"max_sessions\":" << c.max_sessions
      << ",\"max_receive_chunk_bytes\":" << c.max_receive_chunk_bytes
      << ",\"receive_buffer_bytes\":" << c.receive_buffer_bytes
      << ",\"max_message_bytes\":" << c.max_message_bytes
      << ",\"send_buffer_bytes\":" << c.send_buffer_bytes
      << ",\"completion_batch_size\":" << c.completion_batch_size
      << ",\"worker_wait_mode\":" << static_cast<unsigned>(c.worker_wait_mode)
      << ",\"adaptive_spin_count\":" << c.adaptive_spin_count
      << ",\"enable_thread_affinity\":" << c.enable_thread_affinity
      << ",\"enable_metrics\":" << c.enable_metrics
      << ",\"enable_callback_timing\":" << c.enable_callback_timing
      << ",\"graceful_shutdown_timeout_ms\":" << c.graceful_shutdown_timeout_ms
      << ",\"peer_close_drain_timeout_ms\":" << c.peer_close_drain_timeout_ms
      << ",\"idle_timeout_ms\":" << c.idle_timeout_ms
      << ",\"keepalive_time_ms\":" << c.keepalive_time_ms
      << ",\"keepalive_interval_ms\":" << c.keepalive_interval_ms
      << ",\"pageSize\":" << page_size << '}';
}
}  // namespace

int main() {
  SYSTEM_INFO system_info{};
  GetSystemInfo(&system_info);
  rionetlite::ServerConfig base;
  base.port = 32452;
  base.io_worker_count = 2;
  base.max_sessions = 8;
  std::vector<Case> cases;
  const auto add = [&](std::string_view id, auto change) {
    auto config = base;
    change(config);
    cases.push_back({id, config});
  };
  add("valid-base", [](auto&) {});
  add("uneven-sessions", [](auto& c) { c.max_sessions = 9; });
  add("zero-port", [](auto& c) { c.port = 0; });
  add("zero-workers", [](auto& c) { c.io_worker_count = 0; });
  add("too-many-workers", [](auto& c) { c.io_worker_count = 257; });
  add("workers-exceed-sessions", [](auto& c) { c.max_sessions = 1; });
  add("unaligned-receive", [](auto& c) { c.receive_buffer_bytes = 4097; });
  add("unaligned-send", [](auto& c) { c.send_buffer_bytes = 4097; });
  add("oversized-message", [](auto& c) { c.max_message_bytes = 262145; });
  add("zero-batch", [](auto& c) { c.completion_batch_size = 0; });
  add("idle-too-short", [](auto& c) { c.idle_timeout_ms = 999; });
  add("zero-peer-timeout", [](auto& c) { c.peer_close_drain_timeout_ms = 0; });
  add("keepalive-without-interval", [](auto& c) {
    c.keepalive_time_ms = 30000;
    c.keepalive_interval_ms = 0;
  });
  add("cq-too-large", [](auto& c) {
    c.io_worker_count = 1;
    c.max_sessions = 2147483647;
  });
  add("minimum-message", [](auto& c) { c.max_message_bytes = 1; });
  add("enum-not-validated", [](auto& c) {
    c.worker_wait_mode = static_cast<rionetlite::WorkerWaitMode>(99);
  });
  std::cout << std::boolalpha << "{\"schemaVersion\":1,\"cases\":[";
  bool first = true;
  for (const auto& item : cases) {
    if (!first) std::cout << ',';
    first = false;
    std::cout << "{\"id\":\"" << item.id << "\",\"config\":";
    WriteConfig(item.config, system_info.dwPageSize);
    const auto result = rionetlite::ValidateServerConfig(item.config);
    std::cout << ",\"valid\":" << result.has_value();
    if (!result) {
      std::cout << ",\"field\":\"" << result.error().field << '"';
    } else {
      std::cout << ",\"budgets\":{\"sessionsPerWorker\":"
                << result->max_sessions_per_worker
                << ",\"cq\":" << result->completion_queue_capacity
                << ",\"registered\":\"" << result->estimated_registered_bytes
                << "\",\"reserved\":\""
                << result->estimated_reserved_virtual_bytes
                << "\",\"committed\":\"" << result->estimated_committed_bytes
                << "\",\"control\":\"" << result->estimated_control_bytes
                << "\"}";
    }
    std::cout << '}';
  }
  std::cout << "]}\n";
}
