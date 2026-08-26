#pragma once

#include "rionetlite/error.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace rionetlite {

struct SessionId final {
  std::uint32_t index = 0;
  std::uint64_t generation = 0;

  friend bool operator==(const SessionId&, const SessionId&) = default;
};

class EventHandler {
 public:
  virtual ~EventHandler() = default;

  virtual void OnConnected(SessionId session) = 0;

  // data is valid only until this callback returns. The return value is the
  // number of bytes consumed from data.
  virtual std::size_t OnData(SessionId session,
                             std::span<const std::byte> data) = 0;

  virtual void OnClosed(SessionId session, CloseReason reason) = 0;
};

enum class LogSeverity : std::uint8_t {
  kTrace,
  kDebug,
  kInfo,
  kWarning,
  kError,
  kFatal,
};

class Logger {
 public:
  virtual ~Logger() = default;
  virtual void Log(LogSeverity level, std::string_view message) = 0;
};

}  // namespace rionetlite
