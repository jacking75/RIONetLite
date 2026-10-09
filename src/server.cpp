#include "rionetlite/server.h"

#include "../RIONetLite/define.h"
#include "../RIONetLite/i_net_event.h"
#include "../RIONetLite/impl/server_net.h"
#include "detail/session_partition.h"
#include "detail/shutdown_coordinator.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <limits>
#include <mutex>
#include <new>
#include <optional>
#include <shared_mutex>
#include <string_view>
#include <utility>
#include <vector>

namespace rionetlite {
namespace {

// A stack supports nested callbacks and independent servers on the same thread.
class CallbackScope final {
 public:
  explicit CallbackScope(const void* owner) noexcept
      : owner_(owner), previous_(current_) {
    current_ = this;
  }
  ~CallbackScope() { current_ = previous_; }
  static bool Contains(const void* owner) noexcept {
    for (auto* scope = current_; scope != nullptr; scope = scope->previous_) {
      if (scope->owner_ == owner) return true;
    }
    return false;
  }

 private:
  const void* owner_;
  CallbackScope* previous_;
  static inline thread_local CallbackScope* current_ = nullptr;
};

Error LifecycleError(std::string message) {
  return Error{
      .category = ErrorCategory::kLifecycle,
      .code = ErrorCode::kInvalidState,
      .native_code = 0,
      .field = "state",
      .message = std::move(message),
  };
}

Error ConvertLegacyError(const NetErrorCode code,
                         const std::uint32_t native_code = 0) {
  Error error;
  error.native_code = native_code;
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
      return CloseReason::kApplicationRequest;
    case DisconnectReason::kFORCES_CLOSE_SERVER:
      return CloseReason::kServerStopping;
    case DisconnectReason::kZERO_COMPLETION:
      return CloseReason::kPeerClosed;
    case DisconnectReason::kIO_REQUEST_RECV_ERROR:
      return CloseReason::kReceiveError;
    case DisconnectReason::kPARITAL_SEND_COMPLETION:
    case DisconnectReason::kSEND_ERROR:
    case DisconnectReason::kSEND_THREAD_REAL_SEND_ERROR:
    case DisconnectReason::kFORCES_CLOSE_SEND_ERROR:
      return CloseReason::kSendError;
    case DisconnectReason::kRECV_BUFFER_FULL:
    case DisconnectReason::kOVERLOADED:
      return CloseReason::kOverloaded;
    case DisconnectReason::kIDLE_TIMEOUT:
      return CloseReason::kIdleTimeout;
    case DisconnectReason::kPROTOCOL_ERROR:
      return CloseReason::kProtocolError;
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
  target.session_cfg_.max_send_count_per_1sec_ = 1;
  target.session_cfg_.max_send_buffer_size_ =
      static_cast<std::int32_t>(source.send_buffer_bytes);
  target.session_cfg_.keepalive_time_ms_ = source.keepalive_time_ms;
  target.session_cfg_.keepalive_interval_ms_ = source.keepalive_interval_ms;
  target.completion_batch_size_ = source.completion_batch_size;
  target.completion_queue_capacity_ = validated.completion_queue_capacity;
  target.adaptive_spin_count_ = source.adaptive_spin_count;
  target.graceful_shutdown_timeout_ms_ = source.graceful_shutdown_timeout_ms;
  target.peer_close_drain_timeout_ms_ = source.peer_close_drain_timeout_ms;
  target.idle_timeout_ms_ = source.idle_timeout_ms;
  target.worker_wait_mode_ = static_cast<std::uint8_t>(source.worker_wait_mode);
  target.enable_thread_affinity_ = source.enable_thread_affinity;
  return target;
}

std::vector<std::uint16_t> BuildSessionWorkerTable(
    const std::uint32_t max_sessions, const std::uint32_t worker_count) {
  std::vector<std::uint16_t> table(max_sessions, 0);
  for (std::uint32_t worker = 0; worker < worker_count; ++worker) {
    const auto partition =
        detail::PartitionSessions(max_sessions, worker_count, worker);
    for (std::uint32_t offset = 0; offset < partition.count; ++offset) {
      table[partition.first_index + offset] =
          static_cast<std::uint16_t>(worker);
    }
  }
  return table;
}

}  // namespace

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif

struct alignas(64) WorkerMetrics final {
  std::atomic<std::uint64_t> accepted_connections{0};
  std::atomic<std::uint64_t> active_connections{0};
  std::atomic<std::uint64_t> closed_connections{0};
  std::atomic<std::uint64_t> received_bytes{0};
  std::atomic<std::uint64_t> sent_bytes{0};
  std::atomic<std::uint64_t> queued_send_bytes{0};
  std::atomic<std::uint64_t> send_would_block{0};
  std::atomic<std::uint64_t> stale_operations{0};
  std::atomic<std::uint64_t> protocol_errors{0};
  std::atomic<std::uint64_t> receive_operations{0};
  std::atomic<std::uint64_t> send_operations{0};
  std::atomic<std::uint64_t> close_peer{0};
  std::atomic<std::uint64_t> close_application{0};
  std::atomic<std::uint64_t> close_error{0};
  std::atomic<std::uint64_t> corrupt_completion_queues{0};
  std::atomic<std::uint64_t> callback_count{0};
  std::atomic<std::uint64_t> callback_total_nanoseconds{0};
  std::atomic<std::uint64_t> callback_max_nanoseconds{0};
};

static_assert(alignof(WorkerMetrics) == 64);

class Server::Impl final : public INetEventManager {
 public:
  Impl(ValidatedServerConfig config, EventHandler& handler, Logger* logger)
      : config_(std::move(config)),
        handler_(handler),
        logger_(logger),
        pending_receive_bytes_(config_.value.max_sessions, 0),
        session_worker_table_(BuildSessionWorkerTable(
            config_.value.max_sessions, config_.value.io_worker_count)) {
    worker_metrics_.reserve(config_.value.io_worker_count);
    for (std::uint32_t worker = 0; worker < config_.value.io_worker_count;
         ++worker) {
      worker_metrics_.push_back(std::make_unique<WorkerMetrics>());
    }
  }

  ~Impl() override {
    const auto current = state_.load(std::memory_order_acquire);
    if (current == ServerState::kInitialized ||
        current == ServerState::kRunning || current == ServerState::kStopping) {
      static_cast<void>(legacy_server_.End(
          false, config_.value.graceful_shutdown_timeout_ms));
    }
  }

  void Emit(const LogSeverity level, const std::string_view component,
            const SessionId session, const std::string_view operation,
            const ErrorCode error, const std::uint32_t native_error,
            const std::string_view message,
            const std::int32_t worker = -1) const noexcept {
    if (logger_ == nullptr) {
      return;
    }
    CallbackScope callback_scope(this);
    if (!logger_->IsEnabled(level)) return;
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    try {
      logger_->Log(LogEvent{
          .timestamp_unix_ns = static_cast<std::uint64_t>(
              std::chrono::duration_cast<std::chrono::nanoseconds>(now)
                  .count()),
          .level = level,
          .component = component,
          .worker = worker,
          .session = session,
          .operation = operation,
          .error = error,
          .native_error = native_error,
          .message = message,
      });
    } catch (...) {
      // Logging must not turn a recoverable error or noexcept Stop into
      // terminate.
    }
  }

  WorkerMetrics& MetricsForWorker(const std::uint32_t worker) noexcept {
    return *worker_metrics_[std::min<std::size_t>(worker,
                                                  worker_metrics_.size() - 1)];
  }

  // 조회 테이블을 미리 만들어 두면 수신·송신마다 worker를 나눗셈으로 역산하지
  // 않는다.
  WorkerMetrics& MetricsForSession(const std::uint32_t index) noexcept {
    const std::uint32_t bounded_index =
        std::min<std::uint32_t>(index, config_.value.max_sessions - 1);
    return MetricsForWorker(session_worker_table_[bounded_index]);
  }

  [[nodiscard]] std::chrono::steady_clock::time_point CallbackBegin()
      const noexcept {
    return callback_timing_enabled_ ? std::chrono::steady_clock::now()
                                    : std::chrono::steady_clock::time_point{};
  }

  void RecordCallbackDuration(
      WorkerMetrics& metrics,
      const std::chrono::steady_clock::time_point begin) noexcept {
    if (!callback_timing_enabled_) {
      return;
    }
    const auto duration = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - begin)
            .count());
    metrics.callback_count.fetch_add(1, std::memory_order_relaxed);
    metrics.callback_total_nanoseconds.fetch_add(duration,
                                                 std::memory_order_relaxed);
    auto maximum =
        metrics.callback_max_nanoseconds.load(std::memory_order_relaxed);
    while (maximum < duration &&
           !metrics.callback_max_nanoseconds.compare_exchange_weak(
               maximum, duration, std::memory_order_relaxed)) {
    }
  }

  void OnConnect(const std::int32_t index,
                 const std::uint64_t generation) override {
    CallbackScope callback_scope(this);
    WorkerMetrics* metrics = nullptr;
    if (config_.value.enable_metrics) {
      metrics = &MetricsForSession(static_cast<std::uint32_t>(index));
      pending_receive_bytes_[static_cast<std::size_t>(index)] = 0;
      metrics->accepted_connections.fetch_add(1, std::memory_order_relaxed);
      metrics->active_connections.fetch_add(1, std::memory_order_relaxed);
    }
    const SessionId session{.index = static_cast<std::uint32_t>(index),
                            .generation = generation};
    const auto begin = CallbackBegin();
    // 사용자 콜백에서 벗어난 예외가 I/O worker 스택을 뚫고 나가면 서버 전체가
    // std::terminate로 죽는다. 여기서 격리한다.
    try {
      handler_.OnConnected(session);
    } catch (const std::exception& exception) {
      Emit(LogSeverity::kError, "session", session, "connect_callback",
           ErrorCode::kInternalError, 0, exception.what());
    } catch (...) {
      Emit(LogSeverity::kError, "session", session, "connect_callback",
           ErrorCode::kInternalError, 0, "unknown exception");
    }
    if (metrics != nullptr) RecordCallbackDuration(*metrics, begin);
  }

  std::uint32_t OnReceive(const std::int32_t index,
                          const std::uint64_t generation,
                          const std::uint32_t size, char* data) override {
    CallbackScope callback_scope(this);
    const auto bytes = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data), size);
    const SessionId session{.index = static_cast<std::uint32_t>(index),
                            .generation = generation};
    const auto begin = CallbackBegin();
    std::size_t consumed = 0;
    try {
      consumed = handler_.OnData(session, bytes);
    } catch (const std::exception& exception) {
      return RejectReceiveCallback(index, generation, exception.what());
    } catch (...) {
      return RejectReceiveCallback(index, generation, "unknown exception");
    }
    WorkerMetrics* metrics = nullptr;
    if (config_.value.enable_metrics) {
      metrics = &MetricsForSession(static_cast<std::uint32_t>(index));
      RecordCallbackDuration(*metrics, begin);
      auto& pending = pending_receive_bytes_[static_cast<std::size_t>(index)];
      const auto newly_received = size >= pending ? size - pending : size;
      metrics->received_bytes.fetch_add(newly_received,
                                        std::memory_order_relaxed);
      metrics->receive_operations.fetch_add(1, std::memory_order_relaxed);
      pending =
          consumed <= size ? size - static_cast<std::uint32_t>(consumed) : 0;
    }
    if (consumed > size) {
      return RejectReceiveCallback(index, generation,
                                   "handler consumed more bytes than provided");
    }
    return static_cast<std::uint32_t>(consumed);
  }

  std::uint32_t RejectReceiveCallback(const std::int32_t index,
                                      const std::uint64_t generation,
                                      const std::string_view message) {
    if (config_.value.enable_metrics) {
      MetricsForSession(static_cast<std::uint32_t>(index))
          .protocol_errors.fetch_add(1, std::memory_order_relaxed);
    }
    Emit(LogSeverity::kError, "session",
         {static_cast<std::uint32_t>(index), generation}, "receive_callback",
         ErrorCode::kInternalError, 0, message);
    static_cast<void>(legacy_server_.ForcedClose(
        index, generation, DisconnectReason::kPROTOCOL_ERROR));
    return 0;
  }

  void OnClose(const std::int32_t index, const std::uint64_t generation,
               const DisconnectReason reason) override {
    CallbackScope callback_scope(this);
    WorkerMetrics* metrics = nullptr;
    if (config_.value.enable_metrics) {
      metrics = &MetricsForSession(static_cast<std::uint32_t>(index));
      metrics->active_connections.fetch_sub(1, std::memory_order_relaxed);
      metrics->closed_connections.fetch_add(1, std::memory_order_relaxed);
      switch (ConvertCloseReason(reason)) {
        case CloseReason::kPeerClosed:
          metrics->close_peer.fetch_add(1, std::memory_order_relaxed);
          break;
        case CloseReason::kApplicationRequest:
        case CloseReason::kServerStopping:
          metrics->close_application.fetch_add(1, std::memory_order_relaxed);
          break;
        default:
          metrics->close_error.fetch_add(1, std::memory_order_relaxed);
          break;
      }
    }
    const SessionId session{.index = static_cast<std::uint32_t>(index),
                            .generation = generation};
    const auto begin = CallbackBegin();
    try {
      handler_.OnClosed(session, ConvertCloseReason(reason));
    } catch (const std::exception& exception) {
      Emit(LogSeverity::kError, "session", session, "close_callback",
           ErrorCode::kInternalError, 0, exception.what());
    } catch (...) {
      Emit(LogSeverity::kError, "session", session, "close_callback",
           ErrorCode::kInternalError, 0, "unknown exception");
    }
    if (metrics != nullptr) RecordCallbackDuration(*metrics, begin);
  }

  void OnTransportError(const std::int32_t worker, const std::int32_t index,
                        const std::uint64_t generation,
                        const std::string_view operation,
                        const std::uint32_t native_error) override {
    Emit(LogSeverity::kError, "rio",
         {static_cast<std::uint32_t>(index), generation}, operation,
         ErrorCode::kInternalError, native_error,
         "RIO request completed with an error", worker);
  }

  void OnStaleCompletion(const std::int32_t worker, const std::int32_t index,
                         const std::uint64_t generation) override {
    if (config_.value.enable_metrics) {
      MetricsForWorker(worker < 0 ? 0U : static_cast<std::uint32_t>(worker))
          .stale_operations.fetch_add(1, std::memory_order_relaxed);
    }
    Emit(LogSeverity::kWarning, "rio",
         {index < 0 ? 0U : static_cast<std::uint32_t>(index), generation},
         "completion", ErrorCode::kInternalError, 0,
         "stale or invalid RIO completion ignored", worker);
  }

  void OnCorruptCompletionQueue(const std::int32_t worker) override {
    if (config_.value.enable_metrics) {
      MetricsForWorker(worker < 0 ? 0U : static_cast<std::uint32_t>(worker))
          .corrupt_completion_queues.fetch_add(1, std::memory_order_relaxed);
    }
    Emit(LogSeverity::kFatal, "rio", {}, "dequeue", ErrorCode::kInternalError,
         0, "RIO completion queue is corrupt", worker);
  }

  void OnRejectedConnection(const std::uint32_t native_error) override {
    if (config_.value.enable_metrics) {
      rejected_connections_.fetch_add(1, std::memory_order_relaxed);
    }
    Emit(LogSeverity::kWarning, "accept", {}, "accept",
         ErrorCode::kResourceAllocationFailed, native_error,
         "connection was rejected");
  }

  void OnSendCompleted(const std::int32_t index,
                       const std::uint32_t bytes) override {
    if (config_.value.enable_metrics) {
      MetricsForSession(static_cast<std::uint32_t>(index))
          .sent_bytes.fetch_add(bytes, std::memory_order_relaxed);
    }
  }

  ValidatedServerConfig config_;
  EventHandler& handler_;
  Logger* logger_ = nullptr;
  ServerNetManagerImpl legacy_server_;
  std::atomic<ServerState> state_{ServerState::kCreated};
  std::vector<std::unique_ptr<WorkerMetrics>> worker_metrics_;
  alignas(64) std::atomic<std::uint64_t> rejected_connections_{0};
  std::vector<std::uint32_t> pending_receive_bytes_;
  std::vector<std::uint16_t> session_worker_table_;
  const bool callback_timing_enabled_ =
      config_.value.enable_metrics && config_.value.enable_callback_timing;
  std::mutex lifecycle_mutex_;
  std::shared_mutex transport_api_mutex_;
  detail::ShutdownCoordinator shutdown_;
  std::optional<Error> stop_error_;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

std::expected<Server, Error> Server::Create(ServerConfig config,
                                            EventHandler& handler,
                                            Logger* logger) {
  auto validated = ValidateServerConfig(config);
  if (!validated) {
    return std::unexpected(std::move(validated.error()));
  }

  try {
    auto impl = std::make_unique<Impl>(std::move(*validated), handler, logger);

    const NetErrorCode result = impl->legacy_server_.Initialize(
        ToLegacyConfig(impl->config_), impl.get());
    if (result != NetErrorCode::kNONE) {
      return std::unexpected(ConvertLegacyError(
          result, impl->legacy_server_.initialization_native_error()));
    }
    impl->state_.store(ServerState::kInitialized, std::memory_order_release);
    impl->Emit(LogSeverity::kInfo, "server", {}, "create", ErrorCode::kNone, 0,
               "server initialized");
    return Server(std::move(impl));
  } catch (const std::bad_alloc&) {
    return std::unexpected(Error{
        .category = ErrorCategory::kResource,
        .code = ErrorCode::kResourceAllocationFailed,
        .native_code = 0,
        .field = "memory",
        .message = "server resource allocation failed",
    });
  } catch (const std::exception& exception) {
    return std::unexpected(Error{
        .category = ErrorCategory::kInternal,
        .code = ErrorCode::kInternalError,
        .native_code = 0,
        .field = "create",
        .message = exception.what(),
    });
  }
}

Server::Server(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
Server::Server(Server&&) noexcept = default;
Server& Server::operator=(Server&&) noexcept = default;
Server::~Server() = default;

std::expected<void, Error> Server::Start() {
  if (!impl_) {
    return std::unexpected(LifecycleError("server was moved from"));
  }
  if (CallbackScope::Contains(impl_.get())) {
    return std::unexpected(LifecycleError("Start is forbidden in callbacks"));
  }
  std::scoped_lock lifecycle_lock(impl_->lifecycle_mutex_);
  if (impl_->state_.load(std::memory_order_acquire) !=
      ServerState::kInitialized) {
    return std::unexpected(LifecycleError("Start requires initialized state"));
  }
  impl_->state_.store(ServerState::kRunning, std::memory_order_release);

  const NetErrorCode result = impl_->legacy_server_.Start();
  if (result != NetErrorCode::kNONE) {
    impl_->state_.store(ServerState::kStopping, std::memory_order_release);
    {
      std::unique_lock transport_lock(impl_->transport_api_mutex_);
    }
    static_cast<void>(impl_->legacy_server_.End(
        false, impl_->config_.value.graceful_shutdown_timeout_ms));
    impl_->state_.store(ServerState::kStopped, std::memory_order_release);
    return std::unexpected(ConvertLegacyError(result));
  }
  impl_->Emit(LogSeverity::kInfo, "server", {}, "start", ErrorCode::kNone, 0,
              "server started");
  return {};
}

std::expected<void, Error> Server::Stop(const StopMode mode) noexcept {
  if (!impl_) {
    return std::unexpected(LifecycleError("server was moved from"));
  }
  if (CallbackScope::Contains(impl_.get())) {
    return std::unexpected(LifecycleError("Stop is forbidden in callbacks"));
  }

  std::unique_lock lifecycle_lock(impl_->lifecycle_mutex_);
  auto current = impl_->state_.load(std::memory_order_acquire);
  if (current == ServerState::kStopping) {
    lifecycle_lock.unlock();
    impl_->shutdown_.Wait();
    lifecycle_lock.lock();
    if (impl_->stop_error_) {
      return std::unexpected(*impl_->stop_error_);
    }
    return {};
  }
  if (current == ServerState::kStopped) {
    if (impl_->stop_error_) {
      return std::unexpected(*impl_->stop_error_);
    }
    return {};
  }
  if (current != ServerState::kInitialized &&
      current != ServerState::kRunning) {
    return std::unexpected(LifecycleError("Stop requires initialized state"));
  }

  if (!impl_->shutdown_.Begin(mode)) {
    return std::unexpected(LifecycleError("shutdown ownership was lost"));
  }
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(
                            impl_->config_.value.graceful_shutdown_timeout_ms);
  impl_->state_.store(ServerState::kStopping, std::memory_order_release);
  lifecycle_lock.unlock();
  {
    // Wait for callers that observed Running. Release before joining callbacks.
    std::unique_lock transport_lock(impl_->transport_api_mutex_);
  }
  const bool stopped =
      impl_->legacy_server_.End(mode == StopMode::kGraceful, deadline);
  lifecycle_lock.lock();
  if (!stopped) {
    impl_->stop_error_ = Error{
        .category = ErrorCategory::kLifecycle,
        .code = impl_->legacy_server_.corrupt_completion_queue()
                    ? ErrorCode::kInternalError
                    : ErrorCode::kShutdownTimedOut,
        .native_code = 0,
        .field = "shutdown",
        .message =
            impl_->legacy_server_.corrupt_completion_queue()
                ? "RIO completion queue became corrupt"
                : "shutdown exceeded graceful_shutdown_timeout_ms; "
                  "remaining_outstanding=" +
                      std::to_string(
                          impl_->legacy_server_.remaining_outstanding()) +
                      "; remaining_active_sessions=" +
                      std::to_string(
                          impl_->legacy_server_.remaining_active_sessions()),
    };
  }
  impl_->state_.store(ServerState::kStopped, std::memory_order_release);
  lifecycle_lock.unlock();
  impl_->shutdown_.Complete();
  if (impl_->stop_error_) {
    impl_->Emit(LogSeverity::kError, "server", {}, "stop",
                impl_->stop_error_->code, 0, impl_->stop_error_->message);
    return std::unexpected(*impl_->stop_error_);
  }
  impl_->Emit(LogSeverity::kInfo, "server", {}, "stop", ErrorCode::kNone, 0,
              "server stopped");
  return {};
}

SendResult Server::Send(const SessionId session,
                        const std::span<const std::byte> data) noexcept {
  return Send(session, data, {});
}

SendResult Server::Send(const SessionId session,
                        const std::span<const std::byte> header,
                        const std::span<const std::byte> payload) noexcept {
  if (!impl_) return SendResult::kClosing;
  std::shared_lock transport_lock(impl_->transport_api_mutex_);
  if (impl_->state_.load(std::memory_order_acquire) != ServerState::kRunning) {
    return SendResult::kClosing;
  }
  const std::uint64_t total = static_cast<std::uint64_t>(header.size()) +
                              static_cast<std::uint64_t>(payload.size());
  if (total == 0 || total > impl_->config_.value.max_message_bytes) {
    return SendResult::kMessageTooLarge;
  }

  const NetErrorCode result = impl_->legacy_server_.SendData(
      static_cast<std::int32_t>(session.index), session.generation,
      static_cast<std::uint32_t>(header.size()),
      reinterpret_cast<const char*>(header.data()),
      static_cast<std::uint32_t>(payload.size()),
      reinterpret_cast<const char*>(payload.data()));
  switch (result) {
    case NetErrorCode::kNONE:
      if (impl_->config_.value.enable_metrics) {
        auto& metrics = impl_->MetricsForSession(session.index);
        metrics.queued_send_bytes.fetch_add(total, std::memory_order_relaxed);
        metrics.send_operations.fetch_add(1, std::memory_order_relaxed);
      }
      return SendResult::kQueued;
    case NetErrorCode::kSEND_INVALID_SESSION:
      if (impl_->config_.value.enable_metrics) {
        impl_->MetricsForSession(session.index)
            .stale_operations.fetch_add(1, std::memory_order_relaxed);
      }
      return SendResult::kStaleSession;
    case NetErrorCode::kSEND_SESSION_NETWORK_DISABLE:
      return SendResult::kClosing;
    case NetErrorCode::kSEND_THREAD_BUFFER_FULL:
      if (impl_->config_.value.enable_metrics) {
        impl_->MetricsForSession(session.index)
            .send_would_block.fetch_add(1, std::memory_order_relaxed);
      }
      return SendResult::kWouldBlock;
    case NetErrorCode::kSEND_MESSAGE_TOO_LARGE:
      return SendResult::kMessageTooLarge;
    default:
      return SendResult::kUnavailable;
  }
}

bool Server::RequestClose(const SessionId session,
                          const CloseReason reason) noexcept {
  if (!impl_) return false;
  std::shared_lock transport_lock(impl_->transport_api_mutex_);
  if (impl_->state_.load(std::memory_order_acquire) != ServerState::kRunning) {
    return false;
  }
  DisconnectReason legacy_reason = DisconnectReason::kACTIVE;
  if (reason == CloseReason::kProtocolError) {
    legacy_reason = DisconnectReason::kPROTOCOL_ERROR;
    if (impl_->config_.value.enable_metrics) {
      impl_->MetricsForSession(session.index)
          .protocol_errors.fetch_add(1, std::memory_order_relaxed);
    }
  } else if (reason == CloseReason::kOverloaded) {
    legacy_reason = DisconnectReason::kOVERLOADED;
  } else if (reason == CloseReason::kIdleTimeout) {
    legacy_reason = DisconnectReason::kIDLE_TIMEOUT;
  }
  return impl_->legacy_server_.ForcedClose(
      static_cast<std::int32_t>(session.index), session.generation,
      legacy_reason);
}

ServerState Server::state() const noexcept {
  return impl_ ? impl_->state_.load(std::memory_order_acquire)
               : ServerState::kStopped;
}

MetricsSnapshot Server::Metrics() const noexcept {
  if (!impl_ || !impl_->config_.value.enable_metrics) {
    return {};
  }
  MetricsSnapshot snapshot;
  snapshot.rejected_connections =
      impl_->rejected_connections_.load(std::memory_order_relaxed);
  for (const auto& metrics : impl_->worker_metrics_) {
    snapshot.accepted_connections +=
        metrics->accepted_connections.load(std::memory_order_relaxed);
    snapshot.active_connections +=
        metrics->active_connections.load(std::memory_order_relaxed);
    snapshot.closed_connections +=
        metrics->closed_connections.load(std::memory_order_relaxed);
    snapshot.received_bytes +=
        metrics->received_bytes.load(std::memory_order_relaxed);
    snapshot.sent_bytes += metrics->sent_bytes.load(std::memory_order_relaxed);
    snapshot.queued_send_bytes +=
        metrics->queued_send_bytes.load(std::memory_order_relaxed);
    snapshot.send_would_block +=
        metrics->send_would_block.load(std::memory_order_relaxed);
    snapshot.stale_operations +=
        metrics->stale_operations.load(std::memory_order_relaxed);
    snapshot.protocol_errors +=
        metrics->protocol_errors.load(std::memory_order_relaxed);
    snapshot.receive_operations +=
        metrics->receive_operations.load(std::memory_order_relaxed);
    snapshot.send_operations +=
        metrics->send_operations.load(std::memory_order_relaxed);
    snapshot.close_peer += metrics->close_peer.load(std::memory_order_relaxed);
    snapshot.close_application +=
        metrics->close_application.load(std::memory_order_relaxed);
    snapshot.close_error +=
        metrics->close_error.load(std::memory_order_relaxed);
    snapshot.corrupt_completion_queues +=
        metrics->corrupt_completion_queues.load(std::memory_order_relaxed);
    snapshot.callback_count +=
        metrics->callback_count.load(std::memory_order_relaxed);
    snapshot.callback_total_nanoseconds +=
        metrics->callback_total_nanoseconds.load(std::memory_order_relaxed);
    snapshot.callback_max_nanoseconds = std::max(
        snapshot.callback_max_nanoseconds,
        metrics->callback_max_nanoseconds.load(std::memory_order_relaxed));
  }
  return snapshot;
}

}  // namespace rionetlite
