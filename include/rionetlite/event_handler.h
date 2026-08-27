#pragma once

#include "rionetlite/error.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace rionetlite {

/// @api SessionId - 슬롯 인덱스와 재사용 세대를 결합한 세션 식별자다.
struct SessionId final {
  std::uint32_t index = 0;
  std::uint64_t generation = 0;

  friend bool operator==(const SessionId&, const SessionId&) = default;
};

/// @api EventHandler - 네트워크 수명·수신 이벤트 callback 계약이다.
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

/// @api LogSeverity - 구조화 로그의 심각도다.
enum class LogSeverity : std::uint8_t {
  kTrace,
  kDebug,
  kInfo,
  kWarning,
  kError,
  kFatal,
};

/// @api LogEvent - 세션·작업·native 오류를 포함한 로그 이벤트다.
struct LogEvent final {
  std::uint64_t timestamp_unix_ns = 0;
  LogSeverity level = LogSeverity::kInfo;
  std::string_view component;
  std::int32_t worker = -1;
  SessionId session{};
  std::string_view operation;
  ErrorCode error = ErrorCode::kNone;
  std::uint32_t native_error = 0;
  std::string_view message;
};

/// @api Logger - 서버 인스턴스별 구조화 로그 sink다.
class Logger {
 public:
  virtual ~Logger() = default;
  [[nodiscard]] virtual bool IsEnabled(LogSeverity) const noexcept {
    return true;
  }
  virtual void Log(LogSeverity level, std::string_view message) = 0;
  virtual void Log(const LogEvent& event) { Log(event.level, event.message); }
};

}  // namespace rionetlite
