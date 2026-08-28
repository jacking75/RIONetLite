#include <WinSock2.h>

#include "rionetlite/config.h"
#include "rionetlite/packet_decoder.h"
#include "rionetlite/queued_event_adapter.h"
#include "rionetlite/server.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace rionetlite {
namespace {

class NullHandler final : public EventHandler {
 public:
  void OnConnected(SessionId) override {}
  std::size_t OnData(SessionId, std::span<const std::byte> data) override {
    return data.size();
  }
  void OnClosed(SessionId, CloseReason) override {}
};

class CaptureLogger final : public Logger {
 public:
  void Log(LogSeverity, const std::string_view message) override {
    std::scoped_lock lock(mutex_);
    messages_.append(message);
    messages_.push_back('\n');
  }

  [[nodiscard]] std::string messages() const {
    std::scoped_lock lock(mutex_);
    return messages_;
  }

  [[nodiscard]] std::size_t message_count() const {
    std::scoped_lock lock(mutex_);
    return static_cast<std::size_t>(
        std::count(messages_.begin(), messages_.end(), '\n'));
  }

 private:
  mutable std::mutex mutex_;
  std::string messages_;
};

class FramedEchoHandler final : public EventHandler {
 public:
  FramedEchoHandler() : decoder_(1024) {}

  void Attach(Server& server) noexcept { server_ = &server; }
  void OnConnected(SessionId) override {
    connected_.fetch_add(1, std::memory_order_relaxed);
  }
  std::size_t OnData(SessionId session,
                     std::span<const std::byte> data) override {
    data_callbacks_.fetch_add(1, std::memory_order_relaxed);
    received_bytes_.fetch_add(data.size(), std::memory_order_relaxed);
    const DecodeResult result = decoder_.Decode(data, [&](auto packet) {
      if (server_ != nullptr) {
        last_send_result_.store(server_->Send(session, packet),
                                std::memory_order_relaxed);
        decoded_packets_.fetch_add(1, std::memory_order_relaxed);
      }
    });
    if (result.error != DecodeError::kNone && server_ != nullptr) {
      static_cast<void>(
          server_->RequestClose(session, CloseReason::kProtocolError));
      return data.size();
    }
    return result.consumed_bytes;
  }
  void OnClosed(SessionId, CloseReason) override {}

  [[nodiscard]] std::uint64_t connected() const noexcept {
    return connected_.load(std::memory_order_relaxed);
  }
  [[nodiscard]] std::uint64_t data_callbacks() const noexcept {
    return data_callbacks_.load(std::memory_order_relaxed);
  }
  [[nodiscard]] std::uint64_t received_bytes() const noexcept {
    return received_bytes_.load(std::memory_order_relaxed);
  }
  [[nodiscard]] std::uint64_t decoded_packets() const noexcept {
    return decoded_packets_.load(std::memory_order_relaxed);
  }
  [[nodiscard]] SendResult last_send_result() const noexcept {
    return last_send_result_.load(std::memory_order_relaxed);
  }

 private:
  LengthPrefixedDecoder decoder_;
  Server* server_ = nullptr;
  std::atomic<std::uint64_t> connected_{0};
  std::atomic<std::uint64_t> data_callbacks_{0};
  std::atomic<std::uint64_t> received_bytes_{0};
  std::atomic<std::uint64_t> decoded_packets_{0};
  std::atomic<SendResult> last_send_result_{SendResult::kUnavailable};
};

class RecordingHandler final : public EventHandler {
 public:
  void OnConnected(const SessionId session) override {
    std::scoped_lock lock(mutex_);
    connected_.push_back(session);
    condition_.notify_all();
  }

  std::size_t OnData(SessionId, std::span<const std::byte> data) override {
    return data.size();
  }

  void OnClosed(const SessionId session, const CloseReason reason) override {
    std::scoped_lock lock(mutex_);
    closed_.push_back(session);
    close_reasons_.push_back(reason);
    condition_.notify_all();
  }

  bool WaitForConnected(const std::size_t count) {
    std::unique_lock lock(mutex_);
    return condition_.wait_for(lock, std::chrono::seconds(3),
                               [&]() { return connected_.size() >= count; });
  }

  bool WaitForClosed(const std::size_t count) {
    std::unique_lock lock(mutex_);
    return condition_.wait_for(lock, std::chrono::seconds(3),
                               [&]() { return closed_.size() >= count; });
  }

  SessionId connected(const std::size_t index) const {
    std::scoped_lock lock(mutex_);
    return connected_.at(index);
  }

  std::size_t closed_count() const {
    std::scoped_lock lock(mutex_);
    return closed_.size();
  }

  CloseReason close_reason(const std::size_t index) const {
    std::scoped_lock lock(mutex_);
    return close_reasons_.at(index);
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::vector<SessionId> connected_;
  std::vector<SessionId> closed_;
  std::vector<CloseReason> close_reasons_;
};

class OverConsumingHandler final : public EventHandler {
 public:
  void OnConnected(SessionId) override {}

  std::size_t OnData(SessionId, std::span<const std::byte> data) override {
    return data.size() + 1;
  }

  void OnClosed(SessionId, const CloseReason reason) override {
    std::scoped_lock lock(mutex_);
    close_reason_ = reason;
    closed_ = true;
    condition_.notify_all();
  }

  bool WaitForClose() {
    std::unique_lock lock(mutex_);
    return condition_.wait_for(lock, std::chrono::seconds(3),
                               [&]() { return closed_; });
  }

  CloseReason close_reason() const {
    std::scoped_lock lock(mutex_);
    return close_reason_;
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  CloseReason close_reason_ = CloseReason::kNone;
  bool closed_ = false;
};

// OnData가 OnClosed 뒤에 오지 않는지 확인하기 위해 관찰 순서를 기록한다.
class OrderingHandler final : public EventHandler {
 public:
  void Attach(Server& server) noexcept { server_ = &server; }

  void OnConnected(const SessionId session) override {
    Record(QueuedEventType::kConnected);
    std::scoped_lock lock(mutex_);
    session_ = session;
    condition_.notify_all();
  }

  std::size_t OnData(const SessionId session,
                     const std::span<const std::byte> data) override {
    Record(QueuedEventType::kData);
    if (server_ != nullptr && close_on_data_.load(std::memory_order_acquire)) {
      static_cast<void>(server_->RequestClose(session));
    }
    return data.size();
  }

  void OnClosed(SessionId, CloseReason) override {
    Record(QueuedEventType::kClosed);
    std::scoped_lock lock(mutex_);
    closed_ = true;
    condition_.notify_all();
  }

  void set_close_on_data(const bool value) noexcept {
    close_on_data_.store(value, std::memory_order_release);
  }

  bool WaitForConnected() {
    std::unique_lock lock(mutex_);
    return condition_.wait_for(lock, std::chrono::seconds(3),
                               [&]() { return session_.generation != 0; });
  }

  bool WaitForClosed() {
    std::unique_lock lock(mutex_);
    return condition_.wait_for(lock, std::chrono::seconds(3),
                               [&]() { return closed_; });
  }

  SessionId session() const {
    std::scoped_lock lock(mutex_);
    return session_;
  }

  std::vector<QueuedEventType> events() const {
    std::scoped_lock lock(mutex_);
    return events_;
  }

 private:
  void Record(const QueuedEventType type) {
    std::scoped_lock lock(mutex_);
    events_.push_back(type);
  }

  mutable std::mutex mutex_;
  std::condition_variable condition_;
  Server* server_ = nullptr;
  std::atomic<bool> close_on_data_{false};
  std::vector<QueuedEventType> events_;
  SessionId session_{};
  bool closed_ = false;
};

// 콜백에서 벗어난 예외가 I/O worker를 죽이지 않는지 확인한다.
class ThrowingHandler final : public EventHandler {
 public:
  void OnConnected(SessionId) override {}

  std::size_t OnData(SessionId, std::span<const std::byte>) override {
    throw std::runtime_error("handler failure");
  }

  void OnClosed(SessionId, const CloseReason reason) override {
    std::scoped_lock lock(mutex_);
    close_reason_ = reason;
    closed_ = true;
    condition_.notify_all();
  }

  bool WaitForClose() {
    std::unique_lock lock(mutex_);
    return condition_.wait_for(lock, std::chrono::seconds(3),
                               [&]() { return closed_; });
  }

  CloseReason close_reason() const {
    std::scoped_lock lock(mutex_);
    return close_reason_;
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  CloseReason close_reason_ = CloseReason::kNone;
  bool closed_ = false;
};

class WinsockScope final {
 public:
  WinsockScope() { EXPECT_EQ(WSAStartup(MAKEWORD(2, 2), &data_), 0); }
  ~WinsockScope() { WSACleanup(); }

 private:
  WSADATA data_{};
};

std::pair<SOCKET, std::uint16_t> BindEphemeralSocket() {
  SOCKET socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0,
                             WSA_FLAG_OVERLAPPED);
  if (socket == INVALID_SOCKET) {
    return {INVALID_SOCKET, std::uint16_t{0}};
  }
  SOCKADDR_IN address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  if (bind(socket, reinterpret_cast<SOCKADDR*>(&address), sizeof(address)) ==
      SOCKET_ERROR) {
    closesocket(socket);
    return {INVALID_SOCKET, std::uint16_t{0}};
  }
  int size = sizeof(address);
  if (getsockname(socket, reinterpret_cast<SOCKADDR*>(&address), &size) ==
      SOCKET_ERROR) {
    closesocket(socket);
    return {INVALID_SOCKET, std::uint16_t{0}};
  }
  return {socket, ntohs(address.sin_port)};
}

ServerConfig SmallConfig(const std::uint16_t port) {
  ServerConfig config;
  config.port = port;
  config.io_worker_count = 1;
  config.max_sessions = 2;
  config.max_receive_chunk_bytes = 1024;
  config.receive_buffer_bytes = 4096;
  config.max_message_bytes = 1024;
  config.send_buffer_bytes = 4096;
  config.completion_batch_size = 8;
  return config;
}

std::vector<std::byte> MakePacket(std::initializer_list<std::byte> payload) {
  const std::uint32_t size =
      static_cast<std::uint32_t>(payload.size() + sizeof(std::uint32_t));
  std::vector<std::byte> bytes{
      static_cast<std::byte>((size >> 24U) & 0xffU),
      static_cast<std::byte>((size >> 16U) & 0xffU),
      static_cast<std::byte>((size >> 8U) & 0xffU),
      static_cast<std::byte>(size & 0xffU),
  };
  bytes.insert(bytes.end(), payload);
  return bytes;
}

std::vector<std::byte> MakePacket(const std::span<const std::byte> payload) {
  const std::uint32_t size =
      static_cast<std::uint32_t>(payload.size() + sizeof(std::uint32_t));
  std::vector<std::byte> bytes{
      static_cast<std::byte>((size >> 24U) & 0xffU),
      static_cast<std::byte>((size >> 16U) & 0xffU),
      static_cast<std::byte>((size >> 8U) & 0xffU),
      static_cast<std::byte>(size & 0xffU),
  };
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  return bytes;
}

void SetReceiveTimeout(const SOCKET socket, const DWORD timeout_ms = 3000) {
  ASSERT_EQ(setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
                       reinterpret_cast<const char*>(&timeout_ms),
                       sizeof(timeout_ms)),
            0);
}

bool SendAll(const SOCKET socket, std::span<const std::byte> bytes) {
  while (!bytes.empty()) {
    const int sent = send(socket, reinterpret_cast<const char*>(bytes.data()),
                          static_cast<int>(bytes.size()), 0);
    if (sent <= 0) {
      return false;
    }
    bytes = bytes.subspan(static_cast<std::size_t>(sent));
  }
  return true;
}

bool ReceiveAll(const SOCKET socket, std::span<std::byte> bytes) {
  while (!bytes.empty()) {
    const int received = recv(socket, reinterpret_cast<char*>(bytes.data()),
                              static_cast<int>(bytes.size()), 0);
    if (received <= 0) {
      return false;
    }
    bytes = bytes.subspan(static_cast<std::size_t>(received));
  }
  return true;
}

SOCKET ConnectLoopback(const std::uint16_t port) {
  const SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (client == INVALID_SOCKET) {
    return INVALID_SOCKET;
  }
  SOCKADDR_IN address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(port);
  if (connect(client, reinterpret_cast<SOCKADDR*>(&address), sizeof(address)) ==
      SOCKET_ERROR) {
    closesocket(client);
    return INVALID_SOCKET;
  }
  return client;
}

template <typename Predicate>
bool WaitUntil(Predicate predicate, const std::chrono::milliseconds timeout =
                                        std::chrono::milliseconds(3000)) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return predicate();
}

TEST(IntegrationSmokeTest, PublicConfigurationIsAvailable) {
  ServerConfig config;
  config.port = 32452;
  EXPECT_TRUE(ValidateServerConfig(config).has_value());
}

TEST(IntegrationSmokeTest, BindFailureRollsBackAndAllowsRetry) {
  WinsockScope winsock;
  auto [occupied_socket, port] = BindEphemeralSocket();
  ASSERT_NE(occupied_socket, INVALID_SOCKET);
  ASSERT_NE(port, 0);
  ASSERT_EQ(listen(occupied_socket, SOMAXCONN), 0);

  NullHandler handler;
  auto failed = Server::Create(SmallConfig(port), handler);
  ASSERT_FALSE(failed.has_value());
  EXPECT_EQ(failed.error().code, ErrorCode::kBindFailed);

  closesocket(occupied_socket);
  auto server = Server::Create(SmallConfig(port), handler);
  ASSERT_TRUE(server.has_value()) << server.error().message;
  EXPECT_EQ(server->state(), ServerState::kInitialized);
  ASSERT_TRUE(server->Stop(StopMode::kImmediate).has_value());
  EXPECT_EQ(server->state(), ServerState::kStopped);
  EXPECT_TRUE(server->Stop(StopMode::kImmediate).has_value());
}

TEST(IntegrationSmokeTest, StartsAndStopsWithoutConnections) {
  WinsockScope winsock;
  auto [probe_socket, port] = BindEphemeralSocket();
  ASSERT_NE(probe_socket, INVALID_SOCKET);
  closesocket(probe_socket);

  NullHandler handler;
  auto server = Server::Create(SmallConfig(port), handler);
  ASSERT_TRUE(server.has_value()) << server.error().message;
  ASSERT_TRUE(server->Start().has_value());
  EXPECT_EQ(server->state(), ServerState::kRunning);
  ASSERT_TRUE(server->Stop(StopMode::kImmediate).has_value());
  EXPECT_EQ(server->state(), ServerState::kStopped);
}

TEST(IntegrationSmokeTest, MetricsCanBeDisabledWithoutChangingLifecycle) {
  WinsockScope winsock;
  auto [probe_socket, port] = BindEphemeralSocket();
  ASSERT_NE(probe_socket, INVALID_SOCKET);
  closesocket(probe_socket);

  RecordingHandler handler;
  auto config = SmallConfig(port);
  config.enable_metrics = false;
  auto server = Server::Create(config, handler);
  ASSERT_TRUE(server.has_value()) << server.error().message;
  ASSERT_TRUE(server->Start().has_value());
  const SOCKET client = ConnectLoopback(port);
  ASSERT_NE(client, INVALID_SOCKET);
  ASSERT_TRUE(handler.WaitForConnected(1));
  EXPECT_EQ(server->Metrics().accepted_connections, 0U);
  EXPECT_EQ(server->Metrics().active_connections, 0U);
  closesocket(client);
  ASSERT_TRUE(handler.WaitForClosed(1));
  EXPECT_EQ(server->Metrics().closed_connections, 0U);
  EXPECT_TRUE(server->Stop().has_value());
}

TEST(IntegrationSmokeTest, AggregatesMetricsAcrossWorkers) {
  WinsockScope winsock;
  auto [probe_socket, port] = BindEphemeralSocket();
  ASSERT_NE(probe_socket, INVALID_SOCKET);
  closesocket(probe_socket);

  RecordingHandler handler;
  auto config = SmallConfig(port);
  config.io_worker_count = 2;
  config.max_sessions = 4;
  auto server = Server::Create(config, handler);
  ASSERT_TRUE(server.has_value()) << server.error().message;
  ASSERT_TRUE(server->Start().has_value());

  std::array<SOCKET, 4> clients{};
  for (auto& client : clients) {
    client = ConnectLoopback(port);
    ASSERT_NE(client, INVALID_SOCKET);
  }
  ASSERT_TRUE(handler.WaitForConnected(clients.size()));
  auto metrics = server->Metrics();
  EXPECT_EQ(metrics.accepted_connections, clients.size());
  EXPECT_EQ(metrics.active_connections, clients.size());

  for (const SOCKET client : clients) closesocket(client);
  ASSERT_TRUE(handler.WaitForClosed(clients.size()));
  metrics = server->Metrics();
  EXPECT_EQ(metrics.closed_connections, clients.size());
  EXPECT_EQ(metrics.active_connections, 0U);
  EXPECT_GE(metrics.callback_count, clients.size() * 2);
  EXPECT_TRUE(server->Stop().has_value());
}

TEST(IntegrationSmokeTest, TwoServersUseIndependentResourcesAndLoggers) {
  WinsockScope winsock;
  auto [first_probe, first_port] = BindEphemeralSocket();
  ASSERT_NE(first_probe, INVALID_SOCKET);
  closesocket(first_probe);
  auto [second_probe, second_port] = BindEphemeralSocket();
  ASSERT_NE(second_probe, INVALID_SOCKET);
  closesocket(second_probe);
  ASSERT_NE(first_port, second_port);

  NullHandler first_handler;
  NullHandler second_handler;
  CaptureLogger first_logger;
  CaptureLogger second_logger;
  auto first =
      Server::Create(SmallConfig(first_port), first_handler, &first_logger);
  auto second =
      Server::Create(SmallConfig(second_port), second_handler, &second_logger);
  ASSERT_TRUE(first.has_value()) << first.error().message;
  ASSERT_TRUE(second.has_value()) << second.error().message;
  ASSERT_TRUE(first->Start().has_value());
  ASSERT_TRUE(second->Start().has_value());
  EXPECT_TRUE(first->Stop().has_value());
  EXPECT_TRUE(second->Stop().has_value());
  EXPECT_GE(first_logger.message_count(), std::size_t{3});
  EXPECT_GE(second_logger.message_count(), std::size_t{3});
}

TEST(IntegrationSmokeTest, ConcurrentStopCallsWaitForTheSameShutdown) {
  WinsockScope winsock;
  auto [probe, port] = BindEphemeralSocket();
  ASSERT_NE(probe, INVALID_SOCKET);
  closesocket(probe);
  NullHandler handler;
  auto server = Server::Create(SmallConfig(port), handler);
  ASSERT_TRUE(server.has_value()) << server.error().message;
  ASSERT_TRUE(server->Start().has_value());

  std::atomic<std::uint32_t> failures{0};
  std::vector<std::thread> callers;
  for (std::uint32_t i = 0; i < 8; ++i) {
    callers.emplace_back([&]() {
      if (!server->Stop(StopMode::kImmediate).has_value()) {
        failures.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }
  for (auto& caller : callers) {
    caller.join();
  }
  EXPECT_EQ(failures.load(std::memory_order_relaxed), 0U);
  EXPECT_EQ(server->state(), ServerState::kStopped);
}

TEST(IntegrationSmokeTest, ReusesSlotWithNewGenerationAndRejectsStaleId) {
  WinsockScope winsock;
  auto [probe, port] = BindEphemeralSocket();
  ASSERT_NE(probe, INVALID_SOCKET);
  closesocket(probe);
  auto config = SmallConfig(port);
  config.max_sessions = 1;
  RecordingHandler handler;
  auto server = Server::Create(config, handler);
  ASSERT_TRUE(server.has_value()) << server.error().message;
  ASSERT_TRUE(server->Start().has_value());

  const SOCKET first_client = ConnectLoopback(port);
  ASSERT_NE(first_client, INVALID_SOCKET);
  ASSERT_TRUE(handler.WaitForConnected(1));
  const SessionId first = handler.connected(0);
  closesocket(first_client);
  ASSERT_TRUE(handler.WaitForClosed(1));
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  const SOCKET second_client = ConnectLoopback(port);
  ASSERT_NE(second_client, INVALID_SOCKET);
  ASSERT_TRUE(handler.WaitForConnected(2));
  const SessionId second = handler.connected(1);
  EXPECT_EQ(second.index, first.index);
  EXPECT_NE(second.generation, first.generation);
  const std::array payload{std::byte{1}};
  EXPECT_EQ(server->Send(first, payload), SendResult::kStaleSession);
  EXPECT_FALSE(server->RequestClose(first));

  std::vector<std::thread> closers;
  for (std::uint32_t i = 0; i < 8; ++i) {
    closers.emplace_back(
        [&]() { static_cast<void>(server->RequestClose(second)); });
  }
  for (auto& closer : closers) {
    closer.join();
  }
  ASSERT_TRUE(handler.WaitForClosed(2));
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_EQ(handler.closed_count(), std::size_t{2});
  closesocket(second_client);
  EXPECT_TRUE(server->Stop().has_value());
}

TEST(IntegrationSmokeTest, EchoesFragmentedAndCoalescedFrames) {
  WinsockScope winsock;
  auto [probe_socket, port] = BindEphemeralSocket();
  ASSERT_NE(probe_socket, INVALID_SOCKET);
  closesocket(probe_socket);

  FramedEchoHandler handler;
  CaptureLogger logger;
  auto server = Server::Create(SmallConfig(port), handler, &logger);
  ASSERT_TRUE(server.has_value()) << server.error().message;
  handler.Attach(*server);
  ASSERT_TRUE(server->Start().has_value());

  SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_NE(client, INVALID_SOCKET);
  const DWORD timeout_ms = 3000;
  ASSERT_EQ(setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
                       reinterpret_cast<const char*>(&timeout_ms),
                       sizeof(timeout_ms)),
            0);
  SOCKADDR_IN address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(port);
  ASSERT_EQ(
      connect(client, reinterpret_cast<SOCKADDR*>(&address), sizeof(address)),
      0);

  const auto first = MakePacket({std::byte{1}, std::byte{2}, std::byte{3}});
  const auto second = MakePacket({std::byte{4}});
  std::vector<std::byte> expected = first;
  expected.insert(expected.end(), second.begin(), second.end());
  ASSERT_TRUE(SendAll(client, std::span(first).first(2)));
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  std::vector<std::byte> remainder(first.begin() + 2, first.end());
  remainder.insert(remainder.end(), second.begin(), second.end());
  ASSERT_TRUE(SendAll(client, remainder));

  std::vector<std::byte> actual(expected.size());
  EXPECT_TRUE(ReceiveAll(client, actual)) << logger.messages();
  EXPECT_EQ(actual, expected);
  ASSERT_TRUE(WaitUntil([&]() {
    return handler.decoded_packets() == 2 &&
           handler.received_bytes() == expected.size() + 2 &&
           server->Metrics().received_bytes == expected.size();
  })) << "decoded="
      << handler.decoded_packets()
      << " callback_bytes=" << handler.received_bytes()
      << " metric_bytes=" << server->Metrics().received_bytes;
  EXPECT_EQ(handler.connected(), 1);
  EXPECT_GT(handler.data_callbacks(), 0);
  EXPECT_EQ(handler.received_bytes(), expected.size() + 2);
  EXPECT_EQ(handler.decoded_packets(), 2);
  EXPECT_EQ(handler.last_send_result(), SendResult::kQueued);
  closesocket(client);
  EXPECT_TRUE(server->Stop(StopMode::kGraceful).has_value());
}

TEST(IntegrationSmokeTest, GracefulStopDrainsAcceptedEchoBeforeClose) {
  WinsockScope winsock;
  auto [probe, port] = BindEphemeralSocket();
  ASSERT_NE(probe, INVALID_SOCKET);
  closesocket(probe);

  FramedEchoHandler handler;
  auto server = Server::Create(SmallConfig(port), handler);
  ASSERT_TRUE(server.has_value()) << server.error().message;
  handler.Attach(*server);
  ASSERT_TRUE(server->Start().has_value());
  SOCKET client = ConnectLoopback(port);
  ASSERT_NE(client, INVALID_SOCKET);
  SetReceiveTimeout(client);
  const auto packet = MakePacket(
      {std::byte{0x10}, std::byte{0x20}, std::byte{0x30}, std::byte{0x40}});
  ASSERT_TRUE(SendAll(client, packet));
  ASSERT_TRUE(WaitUntil([&] { return handler.decoded_packets() == 1; }));
  ASSERT_TRUE(server->Stop(StopMode::kGraceful).has_value());
  std::vector<std::byte> echoed(packet.size());
  EXPECT_TRUE(ReceiveAll(client, echoed));
  EXPECT_EQ(echoed, packet);
  closesocket(client);
}

TEST(IntegrationSmokeTest, AcceptsMaximumFrameAndRejectsOneByteOverMaximum) {
  WinsockScope winsock;
  auto [probe, port] = BindEphemeralSocket();
  ASSERT_NE(probe, INVALID_SOCKET);
  closesocket(probe);

  FramedEchoHandler handler;
  auto server = Server::Create(SmallConfig(port), handler);
  ASSERT_TRUE(server.has_value()) << server.error().message;
  handler.Attach(*server);
  ASSERT_TRUE(server->Start().has_value());

  SOCKET client = ConnectLoopback(port);
  ASSERT_NE(client, INVALID_SOCKET);
  SetReceiveTimeout(client);
  std::vector<std::byte> payload(1020, std::byte{0x5a});
  const auto maximum = MakePacket(payload);
  ASSERT_EQ(maximum.size(), 1024U);
  ASSERT_TRUE(SendAll(client, maximum));
  std::vector<std::byte> echoed(maximum.size());
  ASSERT_TRUE(ReceiveAll(client, echoed));
  EXPECT_EQ(echoed, maximum);
  closesocket(client);

  client = ConnectLoopback(port);
  ASSERT_NE(client, INVALID_SOCKET);
  const std::array oversized_header{std::byte{0}, std::byte{0}, std::byte{4},
                                    std::byte{1}};
  ASSERT_TRUE(SendAll(client, oversized_header));
  ASSERT_TRUE(WaitUntil([&]() {
    return server->Metrics().protocol_errors == 1 &&
           server->Metrics().active_connections == 0;
  }));
  closesocket(client);
  EXPECT_TRUE(server->Stop().has_value());
}

TEST(IntegrationSmokeTest, GracefulHalfCloseDrainsEchoThenReportsPeerClose) {
  WinsockScope winsock;
  auto [probe, port] = BindEphemeralSocket();
  ASSERT_NE(probe, INVALID_SOCKET);
  closesocket(probe);

  FramedEchoHandler handler;
  auto config = SmallConfig(port);
  config.max_sessions = 64;
  auto server = Server::Create(config, handler);
  ASSERT_TRUE(server.has_value()) << server.error().message;
  handler.Attach(*server);
  ASSERT_TRUE(server->Start().has_value());
  const auto packet = MakePacket({std::byte{9}, std::byte{8}});
  constexpr std::size_t kRounds = 4;
  constexpr std::size_t kConnections = 32;
  for (std::size_t round = 0; round < kRounds; ++round) {
    SCOPED_TRACE(round);
    std::vector<SOCKET> clients;
    clients.reserve(kConnections);
    for (std::size_t index = 0; index < kConnections; ++index) {
      const SOCKET client = ConnectLoopback(port);
      ASSERT_NE(client, INVALID_SOCKET);
      SetReceiveTimeout(client);
      ASSERT_TRUE(SendAll(client, packet));
      ASSERT_EQ(shutdown(client, SD_SEND), 0);
      clients.push_back(client);
    }
    for (const SOCKET client : clients) {
      std::vector<std::byte> echoed(packet.size());
      EXPECT_TRUE(ReceiveAll(client, echoed));
      EXPECT_EQ(echoed, packet);
      std::array<char, 1> end{};
      EXPECT_EQ(recv(client, end.data(), static_cast<int>(end.size()), 0), 0);
      closesocket(client);
    }
    ASSERT_TRUE(
        WaitUntil([&]() { return server->Metrics().active_connections == 0; }));
  }
  EXPECT_EQ(handler.decoded_packets(), kRounds * kConnections);
  EXPECT_TRUE(server->Stop().has_value());
}

TEST(IntegrationSmokeTest, RejectsConnectionsWhenSessionPoolIsFull) {
  WinsockScope winsock;
  auto [probe, port] = BindEphemeralSocket();
  ASSERT_NE(probe, INVALID_SOCKET);
  closesocket(probe);
  auto config = SmallConfig(port);
  config.max_sessions = 1;
  RecordingHandler handler;
  auto server = Server::Create(config, handler);
  ASSERT_TRUE(server.has_value()) << server.error().message;
  ASSERT_TRUE(server->Start().has_value());

  const SOCKET first = ConnectLoopback(port);
  ASSERT_NE(first, INVALID_SOCKET);
  ASSERT_TRUE(handler.WaitForConnected(1));
  const SOCKET second = ConnectLoopback(port);
  ASSERT_NE(second, INVALID_SOCKET);
  ASSERT_TRUE(
      WaitUntil([&]() { return server->Metrics().rejected_connections >= 1; }));
  EXPECT_EQ(server->Metrics().active_connections, 1U);
  closesocket(second);
  closesocket(first);
  EXPECT_TRUE(server->Stop(StopMode::kImmediate).has_value());
}

TEST(IntegrationSmokeTest, ClosesWhenCallbackConsumesBeyondReadableBytes) {
  WinsockScope winsock;
  auto [probe, port] = BindEphemeralSocket();
  ASSERT_NE(probe, INVALID_SOCKET);
  closesocket(probe);
  OverConsumingHandler handler;
  auto server = Server::Create(SmallConfig(port), handler);
  ASSERT_TRUE(server.has_value()) << server.error().message;
  ASSERT_TRUE(server->Start().has_value());

  const SOCKET client = ConnectLoopback(port);
  ASSERT_NE(client, INVALID_SOCKET);
  const std::array payload{std::byte{1}, std::byte{2}, std::byte{3}};
  ASSERT_TRUE(SendAll(client, payload));
  ASSERT_TRUE(handler.WaitForClose());
  EXPECT_EQ(handler.close_reason(), CloseReason::kProtocolError);
  EXPECT_EQ(server->Metrics().protocol_errors, 1U);
  closesocket(client);
  EXPECT_TRUE(server->Stop().has_value());
}

TEST(IntegrationSmokeTest, NeverDeliversDataAfterCloseWhenClosingFromCallback) {
  WinsockScope winsock;
  auto [probe, port] = BindEphemeralSocket();
  ASSERT_NE(probe, INVALID_SOCKET);
  closesocket(probe);
  OrderingHandler handler;
  auto server = Server::Create(SmallConfig(port), handler);
  ASSERT_TRUE(server.has_value()) << server.error().message;
  handler.Attach(*server);
  ASSERT_TRUE(server->Start().has_value());

  const SOCKET client = ConnectLoopback(port);
  ASSERT_NE(client, INVALID_SOCKET);
  ASSERT_TRUE(handler.WaitForConnected());
  handler.set_close_on_data(true);

  // 종료를 요청한 뒤에도 커널이 이미 완료시킨 수신이 남아 있을 수 있다.
  const std::array payload{std::byte{1}, std::byte{2}, std::byte{3}};
  for (int attempt = 0; attempt < 16; ++attempt) {
    if (!SendAll(client, payload)) {
      break;
    }
  }
  ASSERT_TRUE(handler.WaitForClosed());
  closesocket(client);
  EXPECT_TRUE(server->Stop().has_value());

  const auto events = handler.events();
  ASSERT_FALSE(events.empty());
  EXPECT_EQ(events.front(), QueuedEventType::kConnected);
  EXPECT_EQ(events.back(), QueuedEventType::kClosed);
  EXPECT_EQ(std::count(events.begin(), events.end(), QueuedEventType::kClosed),
            1);
}

TEST(IntegrationSmokeTest, IsolatesHandlerExceptionsAndKeepsServing) {
  WinsockScope winsock;
  auto [probe, port] = BindEphemeralSocket();
  ASSERT_NE(probe, INVALID_SOCKET);
  closesocket(probe);
  ThrowingHandler handler;
  auto server = Server::Create(SmallConfig(port), handler);
  ASSERT_TRUE(server.has_value()) << server.error().message;
  ASSERT_TRUE(server->Start().has_value());

  const SOCKET client = ConnectLoopback(port);
  ASSERT_NE(client, INVALID_SOCKET);
  const std::array payload{std::byte{1}, std::byte{2}};
  ASSERT_TRUE(SendAll(client, payload));
  ASSERT_TRUE(handler.WaitForClose());
  EXPECT_EQ(handler.close_reason(), CloseReason::kProtocolError);
  closesocket(client);

  // 예외를 던진 세션만 닫히고 서버는 계속 접속을 받아야 한다.
  EXPECT_EQ(server->state(), ServerState::kRunning);
  const SOCKET second = ConnectLoopback(port);
  EXPECT_NE(second, INVALID_SOCKET);
  if (second != INVALID_SOCKET) {
    closesocket(second);
  }
  EXPECT_TRUE(server->Stop().has_value());
}

TEST(IntegrationSmokeTest, EchoesHeaderAndPayloadSentAsOneScatterMessage) {
  WinsockScope winsock;
  auto [probe, port] = BindEphemeralSocket();
  ASSERT_NE(probe, INVALID_SOCKET);
  closesocket(probe);
  RecordingHandler handler;
  auto server = Server::Create(SmallConfig(port), handler);
  ASSERT_TRUE(server.has_value()) << server.error().message;
  ASSERT_TRUE(server->Start().has_value());

  const SOCKET client = ConnectLoopback(port);
  ASSERT_NE(client, INVALID_SOCKET);
  ASSERT_TRUE(handler.WaitForConnected(1));
  const SessionId session = handler.connected(0);

  const std::array header{std::byte{0}, std::byte{0}, std::byte{0},
                          std::byte{8}};
  const std::array body{std::byte{'p'}, std::byte{'i'}, std::byte{'n'},
                        std::byte{'g'}};
  EXPECT_EQ(server->Send(session, header, body), SendResult::kQueued);

  std::array<std::byte, 8> received{};
  ASSERT_TRUE(ReceiveAll(client, received));
  EXPECT_TRUE(std::equal(header.begin(), header.end(), received.begin()));
  EXPECT_TRUE(std::equal(body.begin(), body.end(), received.begin() + 4));
  closesocket(client);
  EXPECT_TRUE(server->Stop().has_value());
}

TEST(IntegrationSmokeTest, ImmediateStopRacesWithConcurrentSendsWithoutHang) {
  WinsockScope winsock;
  auto [probe, port] = BindEphemeralSocket();
  ASSERT_NE(probe, INVALID_SOCKET);
  closesocket(probe);
  RecordingHandler handler;
  auto server = Server::Create(SmallConfig(port), handler);
  ASSERT_TRUE(server.has_value()) << server.error().message;
  ASSERT_TRUE(server->Start().has_value());
  const SOCKET client = ConnectLoopback(port);
  ASSERT_NE(client, INVALID_SOCKET);
  ASSERT_TRUE(handler.WaitForConnected(1));
  const SessionId session = handler.connected(0);
  const std::array payload{std::byte{1}, std::byte{2}, std::byte{3}};
  std::atomic<bool> run{true};
  std::thread sender([&]() {
    while (run.load(std::memory_order_acquire)) {
      const auto result = server->Send(session, payload);
      if (result == SendResult::kClosing ||
          result == SendResult::kStaleSession) {
        break;
      }
    }
  });
  EXPECT_TRUE(server->Stop(StopMode::kImmediate).has_value());
  run.store(false, std::memory_order_release);
  sender.join();
  closesocket(client);
  EXPECT_EQ(server->state(), ServerState::kStopped);
}

}  // namespace
}  // namespace rionetlite
