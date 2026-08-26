#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace rionetlite {

enum class ErrorCategory : std::uint8_t {
  kNone,
  kConfiguration,
  kLifecycle,
  kWinsock,
  kRio,
  kResource,
  kInternal,
};

enum class ErrorCode : std::uint16_t {
  kNone,
  kInvalidConfiguration,
  kAlreadyInitialized,
  kInvalidState,
  kWinsockStartupFailed,
  kListenSocketCreateFailed,
  kBindFailed,
  kListenFailed,
  kRioUnavailable,
  kRioCompletionQueueCreateFailed,
  kSessionPoolCreateFailed,
  kThreadStartFailed,
  kShutdownTimedOut,
  kResourceAllocationFailed,
  kInternalError,
};

struct Error final {
  ErrorCategory category = ErrorCategory::kNone;
  ErrorCode code = ErrorCode::kNone;
  std::uint32_t native_code = 0;
  std::string field;
  std::string message;
};

enum class SendResult : std::uint8_t {
  kQueued,
  kWouldBlock,
  kMessageTooLarge,
  kStaleSession,
  kClosing,
  kUnavailable,
};

enum class CloseReason : std::uint8_t {
  kNone,
  kPeerClosed,
  kApplicationRequest,
  kProtocolError,
  kReceiveError,
  kSendError,
  kServerStopping,
  kOverloaded,
  kInternalError,
};

std::string_view ToString(ErrorCategory category) noexcept;
std::string_view ToString(ErrorCode code) noexcept;
std::string_view ToString(SendResult result) noexcept;
std::string_view ToString(CloseReason reason) noexcept;

}  // namespace rionetlite
