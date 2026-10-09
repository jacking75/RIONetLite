#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "chat_protocol.h"
#include "rionetlite/queued_event_adapter.h"
#include "rionetlite/server.h"

#include <conio.h>

#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

using namespace rionetlite::chat;
std::atomic<bool> g_stop_requested{false};

BOOL WINAPI HandleConsoleSignal(const DWORD signal) {
  if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT ||
      signal == CTRL_CLOSE_EVENT) {
    g_stop_requested.store(true, std::memory_order_release);
    return TRUE;
  }
  return FALSE;
}

bool ValidUtf8(const std::span<const std::byte> bytes) {
  return bytes.empty() ||
         MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                             reinterpret_cast<const char*>(bytes.data()),
                             static_cast<int>(bytes.size()), nullptr, 0) != 0;
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
        idle_timeout_(idle_timeout),
        broadcasts_(logic_workers) {
    for (auto& inbox : broadcasts_) inbox.slots.resize(events_per_worker);
  }

  rionetlite::EventHandler& handler() noexcept { return adapter_; }
  void Attach(rionetlite::Server& server) noexcept { server_ = &server; }

  void Start() {
    running_.store(true, std::memory_order_release);
    for (std::uint32_t shard = 0; shard < adapter_.shard_count(); ++shard) {
      workers_.emplace_back([this, shard]() { RunShard(shard); });
    }
  }

  bool StopAfterDrain() {
    // Transport has stopped producing events before this is called. Count a
    // worker before TryPop decrements depth, including any resulting fanout.
    while (adapter_.metrics().event_queue_depth != 0 ||
           active_workers_.load(std::memory_order_seq_cst) != 0 ||
           pending_broadcasts_.load(std::memory_order_seq_cst) != 0) {
      std::this_thread::yield();
    }
    running_.store(false, std::memory_order_release);
    for (auto& worker : workers_) {
      worker.join();
    }
    workers_.clear();
    const auto players = active_players_.load(std::memory_order_relaxed);
    std::cout << "Logic drain: active_players=" << players
              << " queued_events=" << adapter_.metrics().event_queue_depth
              << " pending_broadcasts=" << pending_broadcasts_.load() << '\n';
    return players == 0;
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

  struct Broadcast final {
    std::uint32_t room = 0;
    std::size_t payload_bytes = 0;
    std::array<std::byte, 258> payload{};
  };
  struct BroadcastInbox final {
    std::vector<Broadcast> slots;
    std::size_t head = 0;
    std::size_t count = 0;
  };

  bool QueueBroadcast(const std::uint32_t room,
                      const std::span<const std::byte> payload) {
    std::scoped_lock lock(broadcast_mutex_);
    for (const auto& inbox : broadcasts_) {
      if (inbox.count == inbox.slots.size()) return false;
    }
    // Reserve all destinations together. Concurrent senders have the same
    // broadcast order on every shard; saturation cannot cause partial fanout.
    for (auto& inbox : broadcasts_) {
      auto& message =
          inbox.slots[(inbox.head + inbox.count) % inbox.slots.size()];
      message.room = room;
      message.payload_bytes = payload.size();
      std::copy(payload.begin(), payload.end(), message.payload.begin());
      ++inbox.count;
    }
    pending_broadcasts_.fetch_add(broadcasts_.size(),
                                  std::memory_order_seq_cst);
    return true;
  }

  bool PopBroadcast(const std::uint32_t shard, Broadcast& message) {
    std::scoped_lock lock(broadcast_mutex_);
    auto& inbox = broadcasts_[shard];
    if (inbox.count == 0) return false;
    message = inbox.slots[inbox.head];
    inbox.head = (inbox.head + 1) % inbox.slots.size();
    --inbox.count;
    pending_broadcasts_.fetch_sub(1, std::memory_order_seq_cst);
    return true;
  }

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
                     const std::span<const std::byte> payload) {
    player.last_activity = std::chrono::steady_clock::now();
    if (type == 1 && player.phase == Phase::kLogin) {
      if (payload.size() < 2) return false;
      const auto length = ReadU16(payload.data());
      if (length == 0 || length > 32 || payload.size() != 2U + length) {
        return false;
      }
      if (!ValidUtf8(payload.subspan(2))) return false;
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
      if (!ValidUtf8(payload.subspan(2))) return false;
      if (!QueueBroadcast(player.room, payload)) {
        RequestClose(player.id, rionetlite::CloseReason::kOverloaded);
      }
      return true;
    }
    return type == 4 && payload.empty();
  }

  bool ConsumeInput(Player& player) {
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
                  .subspan(consumed + kHeaderBytes, length - kHeaderBytes))) {
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
      active_workers_.fetch_add(1, std::memory_order_seq_cst);
      bool progressed = false;
      rionetlite::QueuedEventHeader event;
      if (adapter_.TryPop(shard, payload, event) ==
          rionetlite::PollEventResult::kEvent) {
        progressed = true;
        if (event.type == rionetlite::QueuedEventType::kConnected) {
          if (!players.contains(event.session.index))
            active_players_.fetch_add(1);
          players[event.session.index] = Player{.id = event.session};
        } else if (event.type == rionetlite::QueuedEventType::kClosed) {
          if (auto found = players.find(event.session.index);
              found != players.end() && found->second.id == event.session) {
            players.erase(found);
            active_players_.fetch_sub(1);
          }
        } else if (auto found = players.find(event.session.index);
                   found != players.end() &&
                   found->second.id == event.session) {
          auto& input = found->second.input;
          input.insert(input.end(), payload.begin(),
                       payload.begin() + event.payload_bytes);
          if (!ConsumeInput(found->second)) {
            RequestClose(event.session,
                         rionetlite::CloseReason::kProtocolError);
          }
        }
      }

      Broadcast message;
      if (PopBroadcast(shard, message)) {
        progressed = true;
        for (const auto& [index, peer] : players) {
          static_cast<void>(index);
          if (peer.phase == Phase::kRoom && peer.room == message.room) {
            static_cast<void>(
                Send(peer.id, 103,
                     std::span(message.payload).first(message.payload_bytes)));
          }
        }
      }

      const auto now = std::chrono::steady_clock::now();
      if (now - last_idle_scan >= std::chrono::seconds(1)) {
        for (const auto& [index, player] : players) {
          static_cast<void>(index);
          if (now - player.last_activity > idle_timeout_) {
            RequestClose(player.id, rionetlite::CloseReason::kIdleTimeout);
          }
        }
        last_idle_scan = now;
      }
      active_workers_.fetch_sub(1, std::memory_order_seq_cst);
      if (!progressed)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  rionetlite::QueuedEventAdapter adapter_;
  rionetlite::Server* server_ = nullptr;
  std::chrono::seconds idle_timeout_;
  std::atomic<bool> running_{false};
  std::vector<std::thread> workers_;
  std::atomic<std::uint32_t> active_workers_{0};
  std::atomic<std::uint32_t> active_players_{0};
  std::atomic<std::uint64_t> pending_broadcasts_{0};
  std::mutex broadcast_mutex_;
  std::vector<BroadcastInbox> broadcasts_;
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
  std::uint32_t io_workers = rionetlite::ServerConfig{}.io_worker_count;
  std::uint32_t idle_seconds = 30;
  std::uint32_t duration_seconds = 0;
  std::filesystem::path stop_file;
  for (int index = 1; index + 1 < argc; ++index) {
    if (std::string_view(argv[index]) == "--stop-file")
      stop_file = argv[index + 1];
  }
  if (!ParseUnsigned(argc, argv, "--port", port) || port > 65535 ||
      !ParseUnsigned(argc, argv, "--logic-workers", logic_workers) ||
      !ParseUnsigned(argc, argv, "--max-sessions", max_sessions) ||
      !ParseUnsigned(argc, argv, "--io-workers", io_workers) ||
      logic_workers > max_sessions ||
      !ParseUnsigned(argc, argv, "--idle-seconds", idle_seconds) ||
      !ParseUnsigned(argc, argv, "--duration", duration_seconds)) {
    std::cerr << "usage: ChatServer [--port N] [--logic-workers N] "
                 "[--max-sessions N] [--io-workers N] [--idle-seconds N] "
                 "[--duration N] [--stop-file PATH]\n";
    return 2;
  }

  rionetlite::ServerConfig config;
  config.port = static_cast<std::uint16_t>(port);
  config.max_sessions = max_sessions;
  config.io_worker_count = io_workers;
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
    if (!stop_file.empty() && std::filesystem::exists(stop_file)) break;
    if (_kbhit() != 0) {
      std::string command;
      std::getline(std::cin, command);
      if (command == "quit") g_stop_requested.store(true);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  SetConsoleCtrlHandler(&HandleConsoleSignal, FALSE);
  const auto stopped = server->Stop(rionetlite::StopMode::kGraceful);
  const bool logic_drained = application.StopAfterDrain();
  return stopped && logic_drained ? 0 : 5;
}
