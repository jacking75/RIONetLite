#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "rionetlite/queued_event_adapter.h"
#include "rionetlite/server.h"

#include <conio.h>

#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

constexpr std::uint16_t kProtocolVersion = 1;
constexpr std::uint32_t kMaxPacketBytes = 1024;
constexpr std::uint32_t kHeaderBytes = 8;
std::atomic<bool> g_stop_requested{false};

BOOL WINAPI HandleConsoleSignal(const DWORD signal) {
  if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT ||
      signal == CTRL_CLOSE_EVENT) {
    g_stop_requested.store(true, std::memory_order_release);
    return TRUE;
  }
  return FALSE;
}

std::uint16_t ReadU16(const std::byte* data) {
  return (static_cast<std::uint16_t>(data[0]) << 8U) |
         static_cast<std::uint16_t>(data[1]);
}

std::uint32_t ReadU32(const std::byte* data) {
  return (static_cast<std::uint32_t>(data[0]) << 24U) |
         (static_cast<std::uint32_t>(data[1]) << 16U) |
         (static_cast<std::uint32_t>(data[2]) << 8U) |
         static_cast<std::uint32_t>(data[3]);
}

// 헤더는 고정 크기이므로 스택에 만들고 payload는 그대로 넘긴다. scatter Send가
// 두 span을 한 메시지로 보내므로 송신마다 임시 버퍼를 할당하지 않는다.
using PacketHeader = std::array<std::byte, kHeaderBytes>;

PacketHeader EncodeHeader(const std::uint16_t type,
                          const std::size_t payload_bytes) {
  const auto length = static_cast<std::uint32_t>(kHeaderBytes + payload_bytes);
  return PacketHeader{
      static_cast<std::byte>((length >> 24U) & 0xffU),
      static_cast<std::byte>((length >> 16U) & 0xffU),
      static_cast<std::byte>((length >> 8U) & 0xffU),
      static_cast<std::byte>(length & 0xffU),
      static_cast<std::byte>((kProtocolVersion >> 8U) & 0xffU),
      static_cast<std::byte>(kProtocolVersion & 0xffU),
      static_cast<std::byte>((type >> 8U) & 0xffU),
      static_cast<std::byte>(type & 0xffU),
  };
}

class ConsoleLogger final : public rionetlite::Logger {
 public:
  void Log(const rionetlite::LogEvent& event) override {
    std::cout << '[' << event.component << "] " << event.message;
    if (event.native_error != 0) {
      std::cout << " native_error=" << event.native_error;
    }
    std::cout << '\n';
  }

  void Log(rionetlite::LogSeverity, std::string_view message) override {
    std::cout << "[RIONetLite] " << message << '\n';
  }
};

class ChatApplication final {
 public:
  ChatApplication(const std::uint32_t logic_workers,
                  const std::uint32_t events_per_worker,
                  const std::uint32_t max_sessions,
                  const std::chrono::seconds idle_timeout)
      : adapter_(logic_workers, events_per_worker, kMaxPacketBytes,
                 max_sessions, rionetlite::EventQueueFullPolicy::kDisconnect,
                 [this](const rionetlite::SessionId session) {
                   RequestClose(session, rionetlite::CloseReason::kOverloaded);
                 }),
        idle_timeout_(idle_timeout) {}

  rionetlite::EventHandler& handler() noexcept { return adapter_; }
  void Attach(rionetlite::Server& server) noexcept { server_ = &server; }

  void Start() {
    running_.store(true, std::memory_order_release);
    for (std::uint32_t shard = 0; shard < adapter_.shard_count(); ++shard) {
      workers_.emplace_back([this, shard]() { RunShard(shard); });
    }
  }

  void StopAfterDrain() {
    while (adapter_.metrics().event_queue_depth != 0) {
      std::this_thread::yield();
    }
    running_.store(false, std::memory_order_release);
    for (auto& worker : workers_) {
      worker.join();
    }
    workers_.clear();
  }

 private:
  enum class Phase : std::uint8_t { kLogin, kLobby, kRoom };
  struct Player final {
    rionetlite::SessionId id{};
    Phase phase = Phase::kLogin;
    std::uint32_t room = 0;
    std::string name;
    std::vector<std::byte> input;
    std::chrono::steady_clock::time_point last_activity =
        std::chrono::steady_clock::now();
  };

  using PlayerMap = std::unordered_map<std::uint32_t, Player>;

  void RequestClose(const rionetlite::SessionId session,
                    const rionetlite::CloseReason reason) {
    if (server_ != nullptr) {
      static_cast<void>(server_->RequestClose(session, reason));
    }
  }

  bool Send(const rionetlite::SessionId session, const std::uint16_t type,
            const std::span<const std::byte> payload) {
    if (server_ == nullptr) return false;
    const PacketHeader header = EncodeHeader(type, payload.size());
    const auto result = server_->Send(session, header, payload);
    if (result == rionetlite::SendResult::kWouldBlock ||
        result == rionetlite::SendResult::kUnavailable) {
      RequestClose(session, rionetlite::CloseReason::kOverloaded);
      return false;
    }
    return result == rionetlite::SendResult::kQueued;
  }

  bool ProcessPacket(Player& player, const std::uint16_t type,
                     const std::span<const std::byte> payload,
                     PlayerMap& players) {
    player.last_activity = std::chrono::steady_clock::now();
    if (type == 1 && player.phase == Phase::kLogin) {
      if (payload.size() < 2) return false;
      const auto length = ReadU16(payload.data());
      if (length == 0 || length > 32 || payload.size() != 2U + length) {
        return false;
      }
      player.name.assign(reinterpret_cast<const char*>(payload.data() + 2),
                         length);
      player.phase = Phase::kLobby;
      return Send(player.id, 101, {});
    }
    if (type == 2 && player.phase == Phase::kLobby && payload.size() == 4) {
      player.room = ReadU32(payload.data());
      player.phase = Phase::kRoom;
      return Send(player.id, 102, payload);
    }
    if (type == 3 && player.phase == Phase::kRoom && payload.size() >= 2) {
      const auto length = ReadU16(payload.data());
      if (length > 256 || payload.size() != 2U + length) return false;
      for (auto& [index, peer] : players) {
        static_cast<void>(index);
        if (peer.phase == Phase::kRoom && peer.room == player.room) {
          static_cast<void>(Send(peer.id, 103, payload));
        }
      }
      return true;
    }
    return type == 4 && payload.empty();
  }

  bool ConsumeInput(Player& player, PlayerMap& players) {
    std::size_t consumed = 0;
    while (player.input.size() - consumed >= kHeaderBytes) {
      const std::byte* header = player.input.data() + consumed;
      const std::uint32_t length = ReadU32(header);
      const std::uint16_t version = ReadU16(header + 4);
      const std::uint16_t type = ReadU16(header + 6);
      if (length < kHeaderBytes || length > kMaxPacketBytes ||
          version != kProtocolVersion) {
        return false;
      }
      if (player.input.size() - consumed < length) break;
      if (!ProcessPacket(
              player, type,
              std::span(player.input)
                  .subspan(consumed + kHeaderBytes, length - kHeaderBytes),
              players)) {
        return false;
      }
      consumed += length;
    }
    if (consumed != 0) {
      player.input.erase(player.input.begin(), player.input.begin() + consumed);
    }
    return player.input.size() < kMaxPacketBytes;
  }

  void RunShard(const std::uint32_t shard) {
    PlayerMap players;
    std::vector<std::byte> payload(kMaxPacketBytes);
    auto last_idle_scan = std::chrono::steady_clock::now();
    while (running_.load(std::memory_order_acquire) ||
           adapter_.metrics().event_queue_depth != 0) {
      rionetlite::QueuedEventHeader event;
      if (adapter_.TryPop(shard, payload, event) ==
          rionetlite::PollEventResult::kEvent) {
        if (event.type == rionetlite::QueuedEventType::kConnected) {
          players[event.session.index] = Player{.id = event.session};
        } else if (event.type == rionetlite::QueuedEventType::kClosed) {
          players.erase(event.session.index);
        } else if (auto found = players.find(event.session.index);
                   found != players.end() &&
                   found->second.id == event.session) {
          auto& input = found->second.input;
          input.insert(input.end(), payload.begin(),
                       payload.begin() + event.payload_bytes);
          if (!ConsumeInput(found->second, players)) {
            RequestClose(event.session,
                         rionetlite::CloseReason::kProtocolError);
          }
        }
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }

      const auto now = std::chrono::steady_clock::now();
      if (now - last_idle_scan >= std::chrono::seconds(1)) {
        for (const auto& [index, player] : players) {
          static_cast<void>(index);
          if (now - player.last_activity > idle_timeout_) {
            RequestClose(player.id,
                         rionetlite::CloseReason::kApplicationRequest);
          }
        }
        last_idle_scan = now;
      }
    }
  }

  rionetlite::QueuedEventAdapter adapter_;
  rionetlite::Server* server_ = nullptr;
  std::chrono::seconds idle_timeout_;
  std::atomic<bool> running_{false};
  std::vector<std::thread> workers_;
};

bool ParseUnsigned(const int argc, char* argv[], const std::string_view name,
                   std::uint32_t& value) {
  for (int index = 1; index + 1 < argc; ++index) {
    if (std::string_view(argv[index]) != name) continue;
    const std::string_view text(argv[index + 1]);
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size() &&
           value != 0;
  }
  return true;
}

}  // namespace

int main(int argc, char* argv[]) {
  std::uint32_t port = 32453;
  std::uint32_t logic_workers = 2;
  std::uint32_t max_sessions = 1024;
  std::uint32_t idle_seconds = 30;
  std::uint32_t duration_seconds = 0;
  if (!ParseUnsigned(argc, argv, "--port", port) || port > 65535 ||
      !ParseUnsigned(argc, argv, "--logic-workers", logic_workers) ||
      !ParseUnsigned(argc, argv, "--max-sessions", max_sessions) ||
      !ParseUnsigned(argc, argv, "--idle-seconds", idle_seconds) ||
      !ParseUnsigned(argc, argv, "--duration", duration_seconds)) {
    std::cerr << "usage: ChatServer [--port N] [--logic-workers N] "
                 "[--max-sessions N] [--idle-seconds N] [--duration N]\n";
    return 2;
  }

  rionetlite::ServerConfig config;
  config.port = static_cast<std::uint16_t>(port);
  config.max_sessions = max_sessions;
  config.max_message_bytes = kMaxPacketBytes;
  config.max_receive_chunk_bytes = 4096;
  ConsoleLogger logger;
  ChatApplication application(logic_workers, 1024, max_sessions,
                              std::chrono::seconds(idle_seconds));
  auto server =
      rionetlite::Server::Create(config, application.handler(), &logger);
  if (!server) {
    std::cerr << "create failed: " << server.error().message << '\n';
    return 3;
  }
  application.Attach(*server);
  application.Start();
  if (auto started = server->Start(); !started) {
    application.StopAfterDrain();
    std::cerr << "start failed: " << started.error().message << '\n';
    return 4;
  }

  SetConsoleCtrlHandler(&HandleConsoleSignal, TRUE);
  std::cout << "ChatServer listening on " << port
            << ". Type quit or press Ctrl+C to stop.\n";
  const auto deadline = duration_seconds == 0
                            ? (std::chrono::steady_clock::time_point::max)()
                            : std::chrono::steady_clock::now() +
                                  std::chrono::seconds(duration_seconds);
  while (!g_stop_requested.load(std::memory_order_acquire)) {
    if (std::chrono::steady_clock::now() >= deadline) break;
    if (_kbhit() != 0) {
      std::string command;
      std::getline(std::cin, command);
      if (command == "quit") g_stop_requested.store(true);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  SetConsoleCtrlHandler(&HandleConsoleSignal, FALSE);
  const auto stopped = server->Stop(rionetlite::StopMode::kGraceful);
  application.StopAfterDrain();
  return stopped ? 0 : 5;
}
