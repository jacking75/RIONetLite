#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "rionetlite/packet_decoder.h"
#include "rionetlite/server.h"

#include <conio.h>

#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <thread>

namespace {
std::atomic<bool> g_stop_requested = false;

BOOL WINAPI HandleConsoleSignal(const DWORD signal) {
  if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT ||
      signal == CTRL_CLOSE_EVENT) {
    g_stop_requested.store(true, std::memory_order_release);
    return TRUE;
  }
  return FALSE;
}

class ConsoleLogger final : public rionetlite::Logger {
 public:
  void Log(rionetlite::LogSeverity, std::string_view message) override {
    std::cout << "[RIONetLite] " << message << '\n';
  }
};

class EchoHandler final : public rionetlite::EventHandler {
 public:
  EchoHandler(const std::uint32_t max_message_bytes,
              const bool observability_enabled)
      : decoder_(max_message_bytes),
        observability_enabled_(observability_enabled) {}
  void Attach(rionetlite::Server& server) noexcept { server_ = &server; }
  void OnConnected(const rionetlite::SessionId session) override {
    if (observability_enabled_) {
      std::cout << "connected: " << session.index << ':' << session.generation
                << '\n';
    }
  }
  std::size_t OnData(const rionetlite::SessionId session,
                     const std::span<const std::byte> data) override {
    const rionetlite::DecodeResult decoded =
        decoder_.Decode(data, [&](const std::span<const std::byte> packet) {
          if (server_ == nullptr) return;
          const auto result = server_->Send(session, packet);
          if (observability_enabled_ &&
              result != rionetlite::SendResult::kQueued) {
            std::cerr << "send rejected: " << session.index << ':'
                      << session.generation
                      << " result=" << rionetlite::ToString(result) << '\n';
          }
          if (result == rionetlite::SendResult::kWouldBlock ||
              result == rionetlite::SendResult::kUnavailable) {
            static_cast<void>(server_->RequestClose(
                session, rionetlite::CloseReason::kOverloaded));
          }
        });
    if (decoded.error != rionetlite::DecodeError::kNone && server_ != nullptr) {
      static_cast<void>(server_->RequestClose(
          session, rionetlite::CloseReason::kProtocolError));
      return data.size();
    }
    return decoded.consumed_bytes;
  }
  void OnClosed(const rionetlite::SessionId session,
                const rionetlite::CloseReason reason) override {
    if (observability_enabled_) {
      std::cout << "closed: " << session.index << ':' << session.generation
                << " reason=" << rionetlite::ToString(reason) << '\n';
    }
  }

 private:
  rionetlite::LengthPrefixedDecoder decoder_;
  rionetlite::Server* server_ = nullptr;
  bool observability_enabled_ = true;
};

bool ParseOptions(const int argc, char* argv[],
                  rionetlite::ServerConfig& config,
                  std::uint32_t& duration_seconds, bool& observability_enabled,
                  std::string& stop_file) {
  for (int index = 1; index < argc; ++index) {
    if (index + 1 >= argc) return false;
    const std::string_view key(argv[index]);
    const std::string_view value(argv[++index]);
    if (key == "--stop-file") {
      stop_file = value;
      continue;
    }
    if (key == "--observability") {
      if (value == "on") {
        observability_enabled = true;
        config.enable_metrics = true;
      } else if (value == "off") {
        observability_enabled = false;
        config.enable_metrics = false;
      } else {
        return false;
      }
      continue;
    }
    if (key == "--affinity") {
      if (value == "on") {
        config.enable_thread_affinity = true;
      } else if (value == "off") {
        config.enable_thread_affinity = false;
      } else {
        return false;
      }
      continue;
    }
    if (key == "--wait-mode") {
      if (value == "polling") {
        config.worker_wait_mode = rionetlite::WorkerWaitMode::kPolling;
      } else if (value == "adaptive") {
        config.worker_wait_mode = rionetlite::WorkerWaitMode::kAdaptive;
      } else if (value == "notification") {
        config.worker_wait_mode = rionetlite::WorkerWaitMode::kNotification;
      } else {
        return false;
      }
      continue;
    }
    std::uint32_t parsed = 0;
    const auto [end, error] =
        std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size()) {
      return false;
    }
    if (key == "--port" && parsed != 0 && parsed <= 65535) {
      config.port = static_cast<std::uint16_t>(parsed);
    } else if (key == "--duration") {
      duration_seconds = parsed;
    } else if (key == "--workers") {
      config.io_worker_count = parsed;
    } else if (key == "--batch") {
      config.completion_batch_size = parsed;
    } else if (key == "--spin") {
      config.adaptive_spin_count = parsed;
    } else if (key == "--max-sessions") {
      config.max_sessions = parsed;
    } else {
      return false;
    }
  }
  return true;
}
}  // namespace

int main(int argc, char* argv[]) {
  rionetlite::ServerConfig config;
  config.port = 32452;
  std::uint32_t duration_seconds = 0;
  bool observability_enabled = true;
  std::string stop_file;
  if (!ParseOptions(argc, argv, config, duration_seconds, observability_enabled,
                    stop_file)) {
    std::cerr << "usage: EchoServer [--port n] [--duration n] [--workers n] "
                 "[--batch n] [--spin n] [--max-sessions n] "
                 "[--wait-mode polling|adaptive|notification] "
                 "[--affinity on|off] [--observability on|off] "
                 "[--stop-file path]\n";
    return 2;
  }
  ConsoleLogger logger;
  EchoHandler handler(config.max_message_bytes, observability_enabled);
  auto server = rionetlite::Server::Create(
      config, handler, observability_enabled ? &logger : nullptr);
  if (!server) {
    std::cerr << "create failed: " << server.error().message << '\n';
    return 3;
  }
  handler.Attach(*server);
  if (auto started = server->Start(); !started) {
    std::cerr << "start failed: " << started.error().message << '\n';
    return 4;
  }

  SetConsoleCtrlHandler(&HandleConsoleSignal, TRUE);
  std::cout << "EchoServer listening on port " << config.port
            << ". Type quit or press Ctrl+C to stop.\n";
  const auto deadline = duration_seconds == 0
                            ? std::chrono::steady_clock::time_point::max()
                            : std::chrono::steady_clock::now() +
                                  std::chrono::seconds(duration_seconds);
  while (!g_stop_requested.load(std::memory_order_acquire)) {
    if (std::chrono::steady_clock::now() >= deadline) break;
    if (!stop_file.empty()) {
      std::error_code error;
      if (std::filesystem::exists(stop_file, error) && !error) break;
    }
    if (_kbhit() != 0) {
      std::string command;
      std::getline(std::cin, command);
      if (command == "quit") {
        g_stop_requested.store(true, std::memory_order_release);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  SetConsoleCtrlHandler(&HandleConsoleSignal, FALSE);
  if (auto stopped = server->Stop(rionetlite::StopMode::kGraceful); !stopped) {
    std::cerr << "stop failed: " << stopped.error().message << '\n';
    return 5;
  }
  return 0;
}
