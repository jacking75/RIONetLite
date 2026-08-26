#include "rionetlite/server.h"

#include "../RIONetLite/define.h"
#include "../RIONetLite/i_net_event.h"
#include "../RIONetLite/impl/server_net.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <mutex>
#include <utility>

namespace rionetlite {
namespace {

std::atomic<bool> g_server_claimed = false;
std::atomic<Logger*> g_logger = nullptr;

void LegacyLogBridge(const int level, const char* message) {
  Logger* logger = g_logger.load(std::memory_order_acquire);
  if (logger == nullptr) {
    return;
  }
  const auto safe_level = std::clamp(level, 0, 5);
  logger->Log(static_cast<LogSeverity>(safe_level),
              message == nullptr ? std::string_view{} : message);
}

Error LifecycleError(std::string message) {
  return Error{
      .category = ErrorCategory::kLifecycle,
      .code = ErrorCode::kInvalidState,
      .native_code = 0,
      .field = "state",
      .message = std::move(message),
  };
}

Error ConvertLegacyError(const NetErrorCode code) {
  Error error;
  error.category = ErrorCategory::kInternal;
  error.code = ErrorCode::kInternalError;
  error.message = "legacy network initialization failed";

  switch (code) {
    case NetErrorCode::kNONE:
      error.category = ErrorCategory::kNone;
      error.code = ErrorCode::kNone;
      error.message.clear();
      break;
    case NetErrorCode::kINVALID_CONFIG:
    case NetErrorCode::kSESSION_COUNT_PER_THREAD_INCORRECT:
      error.category = ErrorCategory::kConfiguration;
      error.code = ErrorCode::kInvalidConfiguration;
      error.message = "legacy configuration validation failed";
      break;
    case NetErrorCode::kWSASTARTUP_FAIL:
      error.category = ErrorCategory::kWinsock;
      error.code = ErrorCode::kWinsockStartupFailed;
      break;
    case NetErrorCode::kLISTEN_SOCKET_CREATE_FAIL:
      error.category = ErrorCategory::kWinsock;
      error.code = ErrorCode::kListenSocketCreateFailed;
      break;
    case NetErrorCode::kLISTEN_SOCKET_BIND_FAIL:
      error.category = ErrorCategory::kWinsock;
      error.code = ErrorCode::kBindFailed;
      break;
    case NetErrorCode::kLISTEN_SOCKET_LISTEN_FAIL:
      error.category = ErrorCategory::kWinsock;
      error.code = ErrorCode::kListenFailed;
      break;
    case NetErrorCode::kRIO_FUNCTION_INIT_FAIL:
      error.category = ErrorCategory::kRio;
      error.code = ErrorCode::kRioUnavailable;
      break;
    case NetErrorCode::kPREPARE_SESSION_POOL_FAIL:
      error.category = ErrorCategory::kResource;
      error.code = ErrorCode::kSessionPoolCreateFailed;
      break;
    case NetErrorCode::kIO_THREAD_START_FAIL:
      error.category = ErrorCategory::kResource;
      error.code = ErrorCode::kThreadStartFailed;
      break;
    default:
      break;
  }
  return error;
}

CloseReason ConvertCloseReason(const DisconnectReason reason) noexcept {
  switch (reason) {
    case DisconnectReason::kACTIVE:
    case DisconnectReason::kFORCES_CLOSE_SERVER:
      return CloseReason::kApplicationRequest;
    case DisconnectReason::kZERO_COMPLETION:
      return CloseReason::kPeerClosed;
    case DisconnectReason::kIO_REQUEST_RECV_ERROR:
      return CloseReason::kReceiveError;
    case DisconnectReason::kPARITAL_SEND_COMPLETION:
    case DisconnectReason::kSEND_ERROR:
    case DisconnectReason::kSEND_THREAD_REAL_SEND_ERROR:
    case DisconnectReason::kFORCES_CLOSE_SEND_ERROR:
      return CloseReason::kSendError;
    case DisconnectReason::kACCEPT_LOOP_REQUEST_QUEUE_ERROR:
    case DisconnectReason::kACCEPT_LOOP_FAIL_ON_CONNECT:
      return CloseReason::kInternalError;
    case DisconnectReason::kNONE:
    case DisconnectReason::kEND:
      return CloseReason::kNone;
  }
  return CloseReason::kInternalError;
}

NetConfig ToLegacyConfig(const ValidatedServerConfig& validated) {
  const ServerConfig& source = validated.value;
  NetConfig target;
  target.port_number_ = source.port;
  target.io_work_thread_count_ =
      static_cast<std::int32_t>(source.io_worker_count);
  target.io_send_thread_count_ = 1;
  target.max_session_count_ = static_cast<std::int32_t>(source.max_sessions);
  target.session_cfg_.max_recv_packet_size_ =
      static_cast<std::int32_t>(source.max_message_bytes);
  target.session_cfg_.max_try_1recv_buffer_size =
      source.max_receive_chunk_bytes;
  target.session_cfg_.max_recv_count_per_1sec_ = 1;
  target.session_cfg_.max_recv_buffer_size_ =
      static_cast<std::int32_t>(source.receive_buffer_bytes);
  target.session_cfg_.max_send_packet_size_ =
      static_cast<std::int32_t>(source.max_message_bytes);
  target.session_cfg_.max_send_count_per_1sec_ =
      static_cast<std::int32_t>(source.max_outstanding_sends_per_session);
  target.session_cfg_.max_send_buffer_size_ =
      static_cast<std::int32_t>(source.send_buffer_bytes);
  return target;
}

}  // namespace

class Server::Impl final : public INetEventManager {
 public:
  Impl(ValidatedServerConfig config, EventHandler& handler, Logger* logger)
      : config_(std::move(config)), handler_(handler), logger_(logger) {}

  ~Impl() override {
    const auto current = state_.load(std::memory_order_acquire);
    if (current == ServerState::kInitialized ||
        current == ServerState::kRunning || current == ServerState::kStopping) {
      legacy_server_.End();
    }
    if (claimed_) {
      if (logger_ != nullptr) {
        g_logger.store(nullptr, std::memory_order_release);
        LogFuncPtr = &DefaultLog;
      }
      g_server_claimed.store(false, std::memory_order_release);
    }
  }

  void OnConnect(const std::int32_t index,
                 const std::uint64_t generation) override {
    handler_.OnConnected(SessionId{.index = static_cast<std::uint32_t>(index),
                                   .generation = generation});
  }

  std::uint32_t OnReceive(const std::int32_t index,
                          const std::uint64_t generation,
                          const std::uint32_t size, char* data) override {
    const auto bytes = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data), size);
    const auto consumed =
        handler_.OnData(SessionId{.index = static_cast<std::uint32_t>(index),
                                  .generation = generation},
                        bytes);
    return static_cast<std::uint32_t>(std::min<std::size_t>(consumed, size));
  }

  void OnClose(const std::int32_t index, const std::uint64_t generation,
               const DisconnectReason reason) override {
    handler_.OnClosed(SessionId{.index = static_cast<std::uint32_t>(index),
                                .generation = generation},
                      ConvertCloseReason(reason));
  }

  ValidatedServerConfig config_;
  EventHandler& handler_;
  Logger* logger_ = nullptr;
  ServerNetManagerImpl legacy_server_;
  std::atomic<ServerState> state_{ServerState::kCreated};
  bool claimed_ = false;
};

std::expected<Server, Error> Server::Create(ServerConfig config,
                                            EventHandler& handler,
                                            Logger* logger) {
  auto validated = ValidateServerConfig(config);
  if (!validated) {
    return std::unexpected(std::move(validated.error()));
  }

  bool expected = false;
  if (!g_server_claimed.compare_exchange_strong(expected, true,
                                                std::memory_order_acq_rel)) {
    return std::unexpected(Error{
        .category = ErrorCategory::kLifecycle,
        .code = ErrorCode::kAlreadyInitialized,
        .native_code = 0,
        .field = "server",
        .message = "only one active RIONetLite Server is supported",
    });
  }

  auto impl = std::make_unique<Impl>(std::move(*validated), handler, logger);
  impl->claimed_ = true;
  if (logger != nullptr) {
    g_logger.store(logger, std::memory_order_release);
    LogFuncPtr = &LegacyLogBridge;
  }

  const NetErrorCode result = impl->legacy_server_.Initialize(
      ToLegacyConfig(impl->config_), impl.get());
  if (result != NetErrorCode::kNONE) {
    return std::unexpected(ConvertLegacyError(result));
  }
  impl->state_.store(ServerState::kInitialized, std::memory_order_release);
  return Server(std::move(impl));
}

Server::Server(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
Server::Server(Server&&) noexcept = default;
Server& Server::operator=(Server&&) noexcept = default;
Server::~Server() = default;

std::expected<void, Error> Server::Start() {
  if (!impl_) {
    return std::unexpected(LifecycleError("server was moved from"));
  }
  ServerState expected = ServerState::kInitialized;
  if (!impl_->state_.compare_exchange_strong(expected, ServerState::kRunning,
                                             std::memory_order_acq_rel)) {
    return std::unexpected(LifecycleError("Start requires initialized state"));
  }

  const NetErrorCode result = impl_->legacy_server_.Start();
  if (result != NetErrorCode::kNONE) {
    impl_->state_.store(ServerState::kStopping, std::memory_order_release);
    impl_->legacy_server_.End();
    impl_->state_.store(ServerState::kStopped, std::memory_order_release);
    return std::unexpected(ConvertLegacyError(result));
  }
  return {};
}

std::expected<void, Error> Server::Stop(const StopMode) noexcept {
  if (!impl_) {
    return std::unexpected(LifecycleError("server was moved from"));
  }

  ServerState current = impl_->state_.load(std::memory_order_acquire);
  while (current == ServerState::kInitialized ||
         current == ServerState::kRunning) {
    if (impl_->state_.compare_exchange_weak(current, ServerState::kStopping,
                                            std::memory_order_acq_rel)) {
      impl_->legacy_server_.End();
      impl_->state_.store(ServerState::kStopped, std::memory_order_release);
      return {};
    }
  }
  if (current == ServerState::kStopped || current == ServerState::kStopping) {
    return {};
  }
  return std::unexpected(LifecycleError("Stop requires initialized state"));
}

SendResult Server::Send(const SessionId session,
                        const std::span<const std::byte> data) noexcept {
  if (!impl_ ||
      impl_->state_.load(std::memory_order_acquire) != ServerState::kRunning) {
    return SendResult::kClosing;
  }
  if (data.empty() || data.size() > impl_->config_.value.max_message_bytes ||
      data.size() > std::numeric_limits<std::uint32_t>::max()) {
    return SendResult::kMessageTooLarge;
  }

  const NetErrorCode result = impl_->legacy_server_.SendData(
      static_cast<std::int32_t>(session.index), session.generation,
      static_cast<std::uint32_t>(data.size()),
      reinterpret_cast<const char*>(data.data()));
  switch (result) {
    case NetErrorCode::kNONE:
      return SendResult::kQueued;
    case NetErrorCode::kSEND_INVALID_SESSION:
      return SendResult::kStaleSession;
    case NetErrorCode::kSEND_SESSION_NETWORK_DISABLE:
      return SendResult::kClosing;
    case NetErrorCode::kSEND_THREAD_BUFFER_FULL:
      return SendResult::kWouldBlock;
    default:
      return SendResult::kUnavailable;
  }
}

bool Server::RequestClose(const SessionId session, const CloseReason) noexcept {
  if (!impl_ ||
      impl_->state_.load(std::memory_order_acquire) != ServerState::kRunning) {
    return false;
  }
  return impl_->legacy_server_.ForcedClose(
      static_cast<std::int32_t>(session.index), session.generation);
}

ServerState Server::state() const noexcept {
  return impl_ ? impl_->state_.load(std::memory_order_acquire)
               : ServerState::kStopped;
}

}  // namespace rionetlite
