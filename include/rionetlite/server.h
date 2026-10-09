#pragma once

#include "rionetlite/config.h"
#include "rionetlite/event_handler.h"
#include "rionetlite/metrics.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>

namespace rionetlite {

/// @api ServerState - 공개 서버 수명 상태다.
enum class ServerState : std::uint8_t {
  kCreated,
  kInitialized,
  kRunning,
  kStopping,
  kStopped,
};

/// @api StopMode - graceful 또는 immediate 종료 정책이다.
enum class StopMode : std::uint8_t {
  kGraceful,
  kImmediate,
};

/// @api Server - move-only RIO TCP 서버 인스턴스다.
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

  // Concurrent/repeated Stop calls are safe. Calls from this server's event or
  // logger callbacks return kInvalidState; request shutdown on an owner thread.
  // Moving or destroying Server requires all application callers to have
  // exited.
  std::expected<void, Error> Stop(StopMode mode = StopMode::kGraceful) noexcept;

  // Thread-safe application entry point. The current transport copies data
  // before returning.
  SendResult Send(SessionId session, std::span<const std::byte> data) noexcept;

  // Sends header and payload as one message without an application-side
  // temporary buffer. Both spans are copied before returning.
  SendResult Send(SessionId session, std::span<const std::byte> header,
                  std::span<const std::byte> payload) noexcept;

  // Thread-safe asynchronous request. False indicates a stale SessionId or a
  // server that is not running.
  bool RequestClose(
      SessionId session,
      CloseReason reason = CloseReason::kApplicationRequest) noexcept;

  // Returns a lock-free lifecycle snapshot.
  [[nodiscard]] ServerState state() const noexcept;

  // Returns an eventually consistent, lock-free metrics snapshot.
  [[nodiscard]] MetricsSnapshot Metrics() const noexcept;

 private:
  class Impl;

  explicit Server(std::unique_ptr<Impl> impl) noexcept;

  std::unique_ptr<Impl> impl_;
};

}  // namespace rionetlite
