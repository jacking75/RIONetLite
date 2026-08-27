#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <Windows.h>

#include <WS2tcpip.h>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

enum class Scenario : std::uint8_t {
  kEcho,
  kSplit,
  kCoalesce,
  kInvalidLength,
  kAbruptClose,
  kHalfClose,
  kSlowRead,
  kNoRead,
};

struct Options final {
  std::string host = "127.0.0.1";
  std::uint16_t port = 32452;
  std::uint32_t connections = 1;
  std::uint32_t ramp_ms = 0;
  std::uint32_t payload_bytes = 128;
  std::uint32_t packets_per_second = 0;
  std::uint32_t duration_seconds = 10;
  std::uint32_t reconnect_count = 0;
  std::uint32_t seed = 1;
  Scenario scenario = Scenario::kEcho;
  std::string json_path = "load-client-result.json";
};

struct Summary final {
  std::atomic<std::uint64_t> connected{0};
  std::atomic<std::uint64_t> connection_failures{0};
  std::atomic<std::uint64_t> sent_packets{0};
  std::atomic<std::uint64_t> received_packets{0};
  std::atomic<std::uint64_t> sent_bytes{0};
  std::atomic<std::uint64_t> received_bytes{0};
  std::atomic<std::uint64_t> data_errors{0};
  std::atomic<std::uint64_t> timeouts{0};
  std::mutex latency_mutex;
  std::vector<std::uint64_t> latency_microseconds;
};

struct Connection;
enum class OperationKind : std::uint8_t { kReceive, kSend };

struct Operation final {
  OVERLAPPED overlapped{};
  WSABUF buffer{};
  OperationKind kind = OperationKind::kReceive;
  Connection* connection = nullptr;
};

struct Connection final {
  SOCKET socket = INVALID_SOCKET;
  Operation receive_operation;
  Operation send_operation;
  std::vector<std::byte> send_buffer;
  std::vector<std::byte> receive_buffer;
  std::size_t send_offset = 0;
  std::size_t receive_size = 0;
  std::uint64_t sequence = 0;
  std::chrono::steady_clock::time_point sent_at{};
  std::chrono::steady_clock::time_point deadline{};
  const Options* options = nullptr;
  Summary* summary = nullptr;
  std::atomic<bool> closed{false};
  std::recursive_mutex io_mutex;
  bool send_completed = false;
};

std::uint32_t Fnv1a(const std::span<const std::byte> bytes) {
  std::uint32_t hash = 2166136261U;
  for (const std::byte value : bytes) {
    hash ^= static_cast<std::uint8_t>(value);
    hash *= 16777619U;
  }
  return hash;
}

void WriteU32(std::byte* data, const std::uint32_t value) {
  data[0] = static_cast<std::byte>((value >> 24U) & 0xffU);
  data[1] = static_cast<std::byte>((value >> 16U) & 0xffU);
  data[2] = static_cast<std::byte>((value >> 8U) & 0xffU);
  data[3] = static_cast<std::byte>(value & 0xffU);
}

void WriteU64(std::byte* data, const std::uint64_t value) {
  WriteU32(data, static_cast<std::uint32_t>(value >> 32U));
  WriteU32(data + 4, static_cast<std::uint32_t>(value));
}

std::vector<std::byte> MakeFrame(const std::uint64_t sequence,
                                 const std::uint32_t payload_bytes,
                                 const std::uint32_t seed) {
  constexpr std::size_t kMetadataBytes = 12;
  std::vector<std::byte> frame(sizeof(std::uint32_t) + kMetadataBytes +
                               payload_bytes);
  WriteU32(frame.data(), static_cast<std::uint32_t>(frame.size()));
  WriteU64(frame.data() + 4, sequence);
  for (std::uint32_t index = 0; index < payload_bytes; ++index) {
    frame[16 + index] =
        static_cast<std::byte>((seed + sequence + index) & 0xffU);
  }
  WriteU32(frame.data() + 12,
           Fnv1a(std::span(frame).subspan(16, payload_bytes)));
  return frame;
}

bool ParseUnsigned(const std::string_view text, std::uint32_t& value) {
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  return error == std::errc{} && end == text.data() + text.size();
}

bool ParseScenario(const std::string_view text, Scenario& scenario) {
  if (text == "echo")
    scenario = Scenario::kEcho;
  else if (text == "split")
    scenario = Scenario::kSplit;
  else if (text == "coalesce")
    scenario = Scenario::kCoalesce;
  else if (text == "invalid-length")
    scenario = Scenario::kInvalidLength;
  else if (text == "abrupt-close")
    scenario = Scenario::kAbruptClose;
  else if (text == "half-close")
    scenario = Scenario::kHalfClose;
  else if (text == "slow-read")
    scenario = Scenario::kSlowRead;
  else if (text == "no-read")
    scenario = Scenario::kNoRead;
  else
    return false;
  return true;
}

std::string_view ScenarioName(const Scenario scenario) noexcept {
  switch (scenario) {
    case Scenario::kEcho:
      return "echo";
    case Scenario::kSplit:
      return "split";
    case Scenario::kCoalesce:
      return "coalesce";
    case Scenario::kInvalidLength:
      return "invalid-length";
    case Scenario::kAbruptClose:
      return "abrupt-close";
    case Scenario::kHalfClose:
      return "half-close";
    case Scenario::kSlowRead:
      return "slow-read";
    case Scenario::kNoRead:
      return "no-read";
  }
  return "unknown";
}

bool ExpectsResponse(const Scenario scenario) noexcept {
  return scenario != Scenario::kNoRead && scenario != Scenario::kAbruptClose &&
         scenario != Scenario::kInvalidLength;
}

bool ParseOptions(const int argc, char* argv[], Options& options) {
  for (int index = 1; index < argc; ++index) {
    const std::string_view key(argv[index]);
    if (index + 1 >= argc) return false;
    const std::string_view value(argv[++index]);
    std::uint32_t number = 0;
    if (key == "--host")
      options.host = value;
    else if (key == "--json")
      options.json_path = value;
    else if (key == "--scenario") {
      if (!ParseScenario(value, options.scenario)) return false;
    } else {
      if (!ParseUnsigned(value, number)) return false;
      if (key == "--port" && number <= 65535) {
        options.port = static_cast<std::uint16_t>(number);
      } else if (key == "--connections")
        options.connections = number;
      else if (key == "--ramp-ms")
        options.ramp_ms = number;
      else if (key == "--payload")
        options.payload_bytes = number;
      else if (key == "--rate")
        options.packets_per_second = number;
      else if (key == "--duration")
        options.duration_seconds = number;
      else if (key == "--reconnect")
        options.reconnect_count = number;
      else if (key == "--seed")
        options.seed = number;
      else
        return false;
    }
  }
  return options.port != 0 && options.connections != 0 &&
         options.connections <= 65535 && options.payload_bytes <= 60 * 1024 &&
         options.duration_seconds != 0;
}

void Close(Connection& connection) {
  std::scoped_lock lock(connection.io_mutex);
  if (!connection.closed.exchange(true, std::memory_order_acq_rel) &&
      connection.socket != INVALID_SOCKET) {
    closesocket(connection.socket);
    connection.socket = INVALID_SOCKET;
  }
}

bool PostReceive(Connection& connection) {
  if (connection.options->scenario == Scenario::kNoRead) return true;
  auto& operation = connection.receive_operation;
  operation.overlapped = {};
  operation.buffer.buf = reinterpret_cast<char*>(
      connection.receive_buffer.data() + connection.receive_size);
  operation.buffer.len = static_cast<ULONG>(connection.receive_buffer.size() -
                                            connection.receive_size);
  DWORD flags = 0;
  DWORD bytes = 0;
  const int result = WSARecv(connection.socket, &operation.buffer, 1, &bytes,
                             &flags, &operation.overlapped, nullptr);
  return result == 0 || WSAGetLastError() == WSA_IO_PENDING;
}

bool PostSend(Connection& connection, const std::size_t chunk_limit = 0) {
  auto& operation = connection.send_operation;
  operation.overlapped = {};
  operation.buffer.buf = reinterpret_cast<char*>(connection.send_buffer.data() +
                                                 connection.send_offset);
  const std::size_t remaining =
      connection.send_buffer.size() - connection.send_offset;
  operation.buffer.len = static_cast<ULONG>(
      chunk_limit == 0 ? remaining : std::min(remaining, chunk_limit));
  DWORD bytes = 0;
  const int result = WSASend(connection.socket, &operation.buffer, 1, &bytes, 0,
                             &operation.overlapped, nullptr);
  return result == 0 || WSAGetLastError() == WSA_IO_PENDING;
}

void PrepareExchange(Connection& connection) {
  connection.send_buffer =
      MakeFrame(connection.sequence, connection.options->payload_bytes,
                connection.options->seed);
  if (connection.options->scenario == Scenario::kCoalesce) {
    auto second =
        MakeFrame(connection.sequence + 1, connection.options->payload_bytes,
                  connection.options->seed);
    connection.send_buffer.insert(connection.send_buffer.end(), second.begin(),
                                  second.end());
  }
  if (connection.options->scenario == Scenario::kInvalidLength) {
    WriteU32(connection.send_buffer.data(), 0xffffffffU);
  }
  connection.send_offset = 0;
  connection.receive_size = 0;
  connection.send_completed = false;
  connection.sent_at = std::chrono::steady_clock::now();
}

void CompleteExchange(Connection& connection) {
  if (!std::equal(connection.send_buffer.begin(), connection.send_buffer.end(),
                  connection.receive_buffer.begin())) {
    connection.summary->data_errors.fetch_add(1, std::memory_order_relaxed);
    Close(connection);
    return;
  }
  const auto latency = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - connection.sent_at)
          .count());
  {
    std::scoped_lock lock(connection.summary->latency_mutex);
    connection.summary->latency_microseconds.push_back(latency);
  }
  const std::uint64_t packet_count =
      connection.options->scenario == Scenario::kCoalesce ? 2 : 1;
  connection.summary->received_packets.fetch_add(packet_count,
                                                 std::memory_order_relaxed);
  connection.sequence += packet_count;
  if (connection.options->scenario == Scenario::kHalfClose ||
      std::chrono::steady_clock::now() >= connection.deadline) {
    Close(connection);
    return;
  }
  if (connection.options->packets_per_second != 0) {
    std::this_thread::sleep_for(std::chrono::microseconds(
        1'000'000 / connection.options->packets_per_second));
  }
  PrepareExchange(connection);
  if (!PostSend(connection, connection.options->scenario == Scenario::kSplit
                                ? connection.send_buffer.size() / 2
                                : 0)) {
    Close(connection);
  }
}

void Worker(HANDLE completion_port, std::atomic<bool>& running) {
  while (running.load(std::memory_order_acquire)) {
    DWORD transferred = 0;
    ULONG_PTR key = 0;
    OVERLAPPED* overlapped = nullptr;
    const BOOL succeeded = GetQueuedCompletionStatus(
        completion_port, &transferred, &key, &overlapped, 100);
    if (overlapped == nullptr) continue;
    auto* operation = reinterpret_cast<Operation*>(overlapped);
    Connection& connection = *operation->connection;
    std::scoped_lock lock(connection.io_mutex);
    if (!succeeded || transferred == 0 || connection.closed.load()) {
      Close(connection);
      continue;
    }
    if (operation->kind == OperationKind::kSend) {
      connection.send_offset += transferred;
      connection.summary->sent_bytes.fetch_add(transferred,
                                               std::memory_order_relaxed);
      if (connection.send_offset < connection.send_buffer.size()) {
        if (!PostSend(connection)) Close(connection);
        continue;
      }
      const std::uint64_t count =
          connection.options->scenario == Scenario::kCoalesce ? 2 : 1;
      connection.summary->sent_packets.fetch_add(count,
                                                 std::memory_order_relaxed);
      connection.send_completed = true;
      if (connection.options->scenario == Scenario::kAbruptClose) {
        LINGER linger{1, 0};
        setsockopt(connection.socket, SOL_SOCKET, SO_LINGER,
                   reinterpret_cast<const char*>(&linger), sizeof(linger));
        Close(connection);
      } else if (connection.options->scenario == Scenario::kHalfClose) {
        static_cast<void>(shutdown(connection.socket, SD_SEND));
        if (connection.receive_size >= connection.send_buffer.size()) {
          CompleteExchange(connection);
        }
      } else if (connection.options->scenario == Scenario::kNoRead &&
                 std::chrono::steady_clock::now() < connection.deadline) {
        ++connection.sequence;
        PrepareExchange(connection);
        if (!PostSend(connection)) Close(connection);
      } else if (connection.receive_size >= connection.send_buffer.size()) {
        CompleteExchange(connection);
        if (!connection.closed.load() && !PostReceive(connection)) {
          Close(connection);
        }
      }
    } else {
      connection.receive_size += transferred;
      connection.summary->received_bytes.fetch_add(transferred,
                                                   std::memory_order_relaxed);
      if (connection.receive_size < connection.send_buffer.size()) {
        if (connection.options->scenario == Scenario::kSlowRead) {
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (!PostReceive(connection)) Close(connection);
      } else {
        if (!connection.send_completed) continue;
        CompleteExchange(connection);
        if (!connection.closed.load() && !PostReceive(connection)) {
          Close(connection);
        }
      }
    }
  }
}

std::uint64_t Percentile(std::vector<std::uint64_t> values,
                         const double percentile) {
  if (values.empty()) return 0;
  std::sort(values.begin(), values.end());
  const auto index = static_cast<std::size_t>(
      percentile * static_cast<double>(values.size() - 1));
  return values[index];
}

std::uint64_t FileTimeTicks(const FILETIME value) noexcept {
  ULARGE_INTEGER ticks{};
  ticks.LowPart = value.dwLowDateTime;
  ticks.HighPart = value.dwHighDateTime;
  return ticks.QuadPart;
}

}  // namespace

int main(int argc, char* argv[]) {
  Options options;
  if (!ParseOptions(argc, argv, options)) {
    std::cerr << "invalid arguments; use --host --port --connections --ramp-ms "
                 "--payload --rate --duration --scenario --seed --reconnect "
                 "--json\n";
    return 2;
  }
  WSADATA winsock{};
  if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) return 3;
  HANDLE completion_port =
      CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
  if (completion_port == nullptr) {
    WSACleanup();
    return 3;
  }

  Summary summary;
  std::atomic<bool> running{true};
  const std::uint32_t worker_count =
      std::clamp<std::uint32_t>(std::thread::hardware_concurrency(), 1, 16);
  std::vector<std::thread> workers;
  for (std::uint32_t index = 0; index < worker_count; ++index) {
    workers.emplace_back(Worker, completion_port, std::ref(running));
  }

  std::vector<std::unique_ptr<Connection>> connections;
  const auto started_at = std::chrono::steady_clock::now();
  const auto deadline =
      started_at + std::chrono::seconds(options.duration_seconds);
  const std::uint32_t rounds = options.reconnect_count + 1;
  const auto round_duration = std::chrono::milliseconds(std::max<std::uint64_t>(
      1, static_cast<std::uint64_t>(options.duration_seconds) * 1000 / rounds));
  for (std::uint32_t round = 0; round < rounds; ++round) {
    const auto round_deadline = std::min(
        deadline,
        started_at + round_duration * static_cast<std::int64_t>(round + 1));
    const std::size_t round_begin = connections.size();
    for (std::uint32_t index = 0; index < options.connections; ++index) {
      auto connection = std::make_unique<Connection>();
      connection->options = &options;
      connection->summary = &summary;
      connection->deadline = round_deadline;
      connection->sequence = (static_cast<std::uint64_t>(round) << 48U) |
                             (static_cast<std::uint64_t>(index) << 32U);
      connection->socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                      nullptr, 0, WSA_FLAG_OVERLAPPED);
      SOCKADDR_IN address{};
      address.sin_family = AF_INET;
      address.sin_port = htons(options.port);
      if (connection->socket == INVALID_SOCKET ||
          InetPtonA(AF_INET, options.host.c_str(), &address.sin_addr) != 1 ||
          connect(connection->socket, reinterpret_cast<SOCKADDR*>(&address),
                  sizeof(address)) == SOCKET_ERROR ||
          CreateIoCompletionPort(reinterpret_cast<HANDLE>(connection->socket),
                                 completion_port, 0, 0) == nullptr) {
        summary.connection_failures.fetch_add(1, std::memory_order_relaxed);
        Close(*connection);
        continue;
      }
      connection->receive_operation.kind = OperationKind::kReceive;
      connection->receive_operation.connection = connection.get();
      connection->send_operation.kind = OperationKind::kSend;
      connection->send_operation.connection = connection.get();
      connection->receive_buffer.resize(128 * 1024);
      PrepareExchange(*connection);
      if (!PostReceive(*connection) ||
          !PostSend(*connection, options.scenario == Scenario::kSplit
                                     ? connection->send_buffer.size() / 2
                                     : 0)) {
        summary.connection_failures.fetch_add(1, std::memory_order_relaxed);
        Close(*connection);
      } else {
        summary.connected.fetch_add(1, std::memory_order_relaxed);
      }
      connections.push_back(std::move(connection));
      if (options.ramp_ms != 0) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(options.ramp_ms / options.connections));
      }
    }
    std::this_thread::sleep_until(round_deadline);
    if (ExpectsResponse(options.scenario)) {
      const auto drain_deadline =
          std::chrono::steady_clock::now() + std::chrono::seconds(5);
      while (std::chrono::steady_clock::now() < drain_deadline) {
        bool all_closed = true;
        for (std::size_t index = round_begin; index < connections.size();
             ++index) {
          if (!connections[index]->closed.load(std::memory_order_acquire)) {
            all_closed = false;
            break;
          }
        }
        if (all_closed) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
    for (std::size_t index = round_begin; index < connections.size(); ++index) {
      Close(*connections[index]);
    }
    if (round + 1 < rounds) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
  for (auto& connection : connections) Close(*connection);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  running.store(false, std::memory_order_release);
  for (std::uint32_t index = 0; index < worker_count; ++index) {
    PostQueuedCompletionStatus(completion_port, 0, 0, nullptr);
  }
  for (auto& worker : workers) worker.join();
  CloseHandle(completion_port);
  WSACleanup();

  std::vector<std::uint64_t> latencies;
  {
    std::scoped_lock lock(summary.latency_mutex);
    latencies = summary.latency_microseconds;
  }
  const auto p50 = Percentile(latencies, 0.50);
  const auto p95 = Percentile(latencies, 0.95);
  const auto p99 = Percentile(latencies, 0.99);
  const auto connected = summary.connected.load(std::memory_order_relaxed);
  const auto connection_failures =
      summary.connection_failures.load(std::memory_order_relaxed);
  const auto sent_packets =
      summary.sent_packets.load(std::memory_order_relaxed);
  const auto received_packets =
      summary.received_packets.load(std::memory_order_relaxed);
  const auto sent_bytes = summary.sent_bytes.load(std::memory_order_relaxed);
  const auto received_bytes =
      summary.received_bytes.load(std::memory_order_relaxed);
  const auto data_errors = summary.data_errors.load(std::memory_order_relaxed);
  const bool expects_response = ExpectsResponse(options.scenario);
  const std::uint64_t dropped_packets =
      expects_response && sent_packets > received_packets
          ? sent_packets - received_packets
          : 0;
  const std::uint64_t timeout_count = dropped_packets;
  const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - started_at)
                              .count();
  FILETIME created{};
  FILETIME exited{};
  FILETIME kernel{};
  FILETIME user{};
  static_cast<void>(
      GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user));
  const auto kernel_cpu_ms = FileTimeTicks(kernel) / 10'000;
  const auto user_cpu_ms = FileTimeTicks(user) / 10'000;
  const auto packets_per_second =
      elapsed_ms == 0 ? 0 : received_packets * 1000 / elapsed_ms;
  const auto bytes_per_second =
      elapsed_ms == 0 ? 0 : received_bytes * 1000 / elapsed_ms;
  const std::uint64_t bounded_buffer_bytes =
      static_cast<std::uint64_t>(connections.size()) *
      (128ULL * 1024ULL + 2ULL * (options.payload_bytes + 16ULL));
  std::cout << "scenario=" << ScenarioName(options.scenario)
            << " connected=" << connected << " sent_packets=" << sent_packets
            << " received_packets=" << received_packets
            << " dropped_packets=" << dropped_packets
            << " data_errors=" << data_errors << " p50_us=" << p50
            << " p95_us=" << p95 << " p99_us=" << p99
            << " packets_per_second=" << packets_per_second
            << " bytes_per_second=" << bytes_per_second
            << " cpu_ms=" << kernel_cpu_ms + user_cpu_ms << '\n';
  std::ofstream json(options.json_path, std::ios::trunc);
  json << "{\n  \"scenario\": \"" << ScenarioName(options.scenario)
       << "\",\n  \"connected\": " << connected
       << ",\n  \"connection_failures\": " << connection_failures
       << ",\n  \"sent_packets\": " << sent_packets
       << ",\n  \"received_packets\": " << received_packets
       << ",\n  \"dropped_packets\": " << dropped_packets
       << ",\n  \"sent_bytes\": " << sent_bytes
       << ",\n  \"received_bytes\": " << received_bytes
       << ",\n  \"data_errors\": " << data_errors << ",\n  \"p50_us\": " << p50
       << ",\n  \"timeouts\": " << timeout_count << ",\n  \"p95_us\": " << p95
       << ",\n  \"p99_us\": " << p99 << ",\n  \"elapsed_ms\": " << elapsed_ms
       << ",\n  \"packets_per_second\": " << packets_per_second
       << ",\n  \"bytes_per_second\": " << bytes_per_second
       << ",\n  \"kernel_cpu_ms\": " << kernel_cpu_ms
       << ",\n  \"user_cpu_ms\": " << user_cpu_ms
       << ",\n  \"worker_count\": " << worker_count
       << ",\n  \"bounded_buffer_bytes\": " << bounded_buffer_bytes
       << ",\n  \"reconnect_count\": " << options.reconnect_count
       << ",\n  \"seed\": " << options.seed << "\n}\n";
  if (!json) return 2;
  if (connection_failures != 0) return 3;
  if (data_errors != 0) return 4;
  if (timeout_count != 0) return 5;
  return 0;
}
