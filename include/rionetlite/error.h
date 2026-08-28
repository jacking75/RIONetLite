#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace rionetlite {

/// @api ErrorCategory - 오류가 발생한 계층을 분류한다.
enum class ErrorCategory : std::uint8_t {
  kNone,
  kConfiguration,
  kLifecycle,
  kWinsock,
  kRio,
  kResource,
  kInternal,
};

/// @api ErrorCode - 공개 API의 안정된 오류 코드다.
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

/// @api Error - 라이브러리·native 오류와 진단 문맥이다.
struct Error final {
  ErrorCategory category = ErrorCategory::kNone;
  ErrorCode code = ErrorCode::kNone;
  std::uint32_t native_code = 0;
  std::string field;
  std::string message;
};

/// @api SendResult - 비동기 송신 요청의 즉시 결과다.
enum class SendResult : std::uint8_t {
  kQueued,
  kWouldBlock,
  kMessageTooLarge,
  kStaleSession,
  kClosing,
  kUnavailable,
};

/// @api CloseReason - 세션이 닫힌 원인을 나타낸다.
enum class CloseReason : std::uint8_t {
  kNone,
  kPeerClosed,
  kApplicationRequest,
  kProtocolError,
  kReceiveError,
  kSendError,
  kServerStopping,
  kOverloaded,
  kIdleTimeout,
  kInternalError,
};

/// @api ToString - 공개 enum을 안정된 문자열로 변환한다.
std::string_view ToString(ErrorCategory category) noexcept;
std::string_view ToString(ErrorCode code) noexcept;
std::string_view ToString(SendResult result) noexcept;
std::string_view ToString(CloseReason reason) noexcept;

}  // namespace rionetlite
