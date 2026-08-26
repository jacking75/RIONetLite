#pragma once

#include "rionetlite/config.h"
#include "rionetlite/event_handler.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>

namespace rionetlite {

enum class ServerState : std::uint8_t {
  kCreated,
  kInitialized,
  kRunning,
  kStopping,
  kStopped,
};

enum class StopMode : std::uint8_t {
  kGraceful,
  kImmediate,
};

class Server final {
 public:
  // Validates config and acquires the resources required for kInitialized.
  // handler and logger must outlive this Server instance.
  static std::expected<Server, Error> Create(ServerConfig config,
                                             EventHandler& handler,
                                             Logger* logger = nullptr);

  Server(Server&& other) noexcept;
  Server& operator=(Server&& other) noexcept;
  ~Server();

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  // Starts accept and I/O workers. Valid only in kInitialized.
  std::expected<void, Error> Start();

  // Repeated calls after the first successful Stop are safe. T08 extends
  // kGraceful with complete outstanding-I/O draining.
  std::expected<void, Error> Stop(StopMode mode = StopMode::kGraceful) noexcept;

  // Thread-safe application entry point. The current transport copies data
  // before returning.
  SendResult Send(SessionId session, std::span<const std::byte> data) noexcept;

  // Thread-safe asynchronous request. False indicates a stale SessionId or a
  // server that is not running.
  bool RequestClose(
      SessionId session,
      CloseReason reason = CloseReason::kApplicationRequest) noexcept;

  // Returns a lock-free lifecycle snapshot.
  [[nodiscard]] ServerState state() const noexcept;

 private:
  class Impl;

  explicit Server(std::unique_ptr<Impl> impl) noexcept;

  std::unique_ptr<Impl> impl_;
};

}  // namespace rionetlite
