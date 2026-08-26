#include "rionetlite/error.h"

namespace rionetlite {

std::string_view ToString(const ErrorCategory category) noexcept {
  switch (category) {
    case ErrorCategory::kNone:
      return "none";
    case ErrorCategory::kConfiguration:
      return "configuration";
    case ErrorCategory::kLifecycle:
      return "lifecycle";
    case ErrorCategory::kWinsock:
      return "winsock";
    case ErrorCategory::kRio:
      return "rio";
    case ErrorCategory::kResource:
      return "resource";
    case ErrorCategory::kInternal:
      return "internal";
  }
  return "unknown";
}

std::string_view ToString(const ErrorCode code) noexcept {
  switch (code) {
    case ErrorCode::kNone:
      return "none";
    case ErrorCode::kInvalidConfiguration:
      return "invalid_configuration";
    case ErrorCode::kAlreadyInitialized:
      return "already_initialized";
    case ErrorCode::kInvalidState:
      return "invalid_state";
    case ErrorCode::kWinsockStartupFailed:
      return "winsock_startup_failed";
    case ErrorCode::kListenSocketCreateFailed:
      return "listen_socket_create_failed";
    case ErrorCode::kBindFailed:
      return "bind_failed";
    case ErrorCode::kListenFailed:
      return "listen_failed";
    case ErrorCode::kRioUnavailable:
      return "rio_unavailable";
    case ErrorCode::kRioCompletionQueueCreateFailed:
      return "rio_completion_queue_create_failed";
    case ErrorCode::kSessionPoolCreateFailed:
      return "session_pool_create_failed";
    case ErrorCode::kThreadStartFailed:
      return "thread_start_failed";
    case ErrorCode::kShutdownTimedOut:
      return "shutdown_timed_out";
    case ErrorCode::kResourceAllocationFailed:
      return "resource_allocation_failed";
    case ErrorCode::kInternalError:
      return "internal_error";
  }
  return "unknown";
}

std::string_view ToString(const SendResult result) noexcept {
  switch (result) {
    case SendResult::kQueued:
      return "queued";
    case SendResult::kWouldBlock:
      return "would_block";
    case SendResult::kMessageTooLarge:
      return "message_too_large";
    case SendResult::kStaleSession:
      return "stale_session";
    case SendResult::kClosing:
      return "closing";
    case SendResult::kUnavailable:
      return "unavailable";
  }
  return "unknown";
}

std::string_view ToString(const CloseReason reason) noexcept {
  switch (reason) {
    case CloseReason::kNone:
      return "none";
    case CloseReason::kPeerClosed:
      return "peer_closed";
    case CloseReason::kApplicationRequest:
      return "application_request";
    case CloseReason::kProtocolError:
      return "protocol_error";
    case CloseReason::kReceiveError:
      return "receive_error";
    case CloseReason::kSendError:
      return "send_error";
    case CloseReason::kServerStopping:
      return "server_stopping";
    case CloseReason::kOverloaded:
      return "overloaded";
    case CloseReason::kInternalError:
      return "internal_error";
  }
  return "unknown";
}

}  // namespace rionetlite
