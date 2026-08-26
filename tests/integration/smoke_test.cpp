#include <WinSock2.h>

#include "rionetlite/config.h"
#include "rionetlite/server.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

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
  config.max_outstanding_sends_per_session = 2;
  config.completion_batch_size = 8;
  return config;
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

}  // namespace
}  // namespace rionetlite
