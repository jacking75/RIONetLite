#include "../RIONetLite/server_net.h"

#include "rionetlite/server.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>

namespace rionetlite {
namespace {

DisconnectReason ToLegacyReason(const CloseReason reason) noexcept {
  switch (reason) {
    case CloseReason::kNone:
      return DisconnectReason::kNONE;
    case CloseReason::kPeerClosed:
      return DisconnectReason::kZERO_COMPLETION;
    case CloseReason::kApplicationRequest:
      return DisconnectReason::kFORCES_CLOSE_SERVER;
    case CloseReason::kReceiveError:
    case CloseReason::kProtocolError:
      return DisconnectReason::kIO_REQUEST_RECV_ERROR;
    case CloseReason::kSendError:
      return DisconnectReason::kSEND_ERROR;
    case CloseReason::kServerStopping:
    case CloseReason::kOverloaded:
    case CloseReason::kInternalError:
      return DisconnectReason::kACTIVE;
  }
  return DisconnectReason::kACTIVE;
}

ServerConfig ToServerConfig(const NetConfig& source) {
  ServerConfig target;
  target.port = source.port_number_;
  target.io_worker_count =
      static_cast<std::uint32_t>(source.io_work_thread_count_);
  target.max_sessions = static_cast<std::uint32_t>(source.max_session_count_);
  target.max_receive_chunk_bytes =
      source.session_cfg_.max_try_1recv_buffer_size;
  target.receive_buffer_bytes =
      static_cast<std::uint32_t>(source.session_cfg_.max_recv_buffer_size_);
  target.max_message_bytes = static_cast<std::uint32_t>(
      std::max(source.session_cfg_.max_recv_packet_size_,
               source.session_cfg_.max_send_packet_size_));
  target.send_buffer_bytes =
      static_cast<std::uint32_t>(source.session_cfg_.max_send_buffer_size_);
  target.max_outstanding_sends_per_session =
      static_cast<std::uint32_t>(source.session_cfg_.max_send_count_per_1sec_);
  return target;
}

}  // namespace

class ServerNetManager::Impl final : public EventHandler {
 public:
  void OnConnected(const SessionId session) override {
    event_manager_->OnConnect(static_cast<std::int32_t>(session.index),
                              session.generation);
  }

  std::size_t OnData(const SessionId session,
                     const std::span<const std::byte> data) override {
    return event_manager_->OnReceive(
        static_cast<std::int32_t>(session.index), session.generation,
        static_cast<std::uint32_t>(data.size()),
        reinterpret_cast<char*>(const_cast<std::byte*>(data.data())));
  }

  void OnClosed(const SessionId session, const CloseReason reason) override {
    event_manager_->OnClose(static_cast<std::int32_t>(session.index),
                            session.generation, ToLegacyReason(reason));
  }

  INetEventManager* event_manager_ = nullptr;
  std::optional<Server> server_;
};

ServerNetManager::ServerNetManager() : impl_(std::make_unique<Impl>()) {}
ServerNetManager::~ServerNetManager() = default;
ServerNetManager::ServerNetManager(ServerNetManager&&) noexcept = default;
ServerNetManager& ServerNetManager::operator=(ServerNetManager&&) noexcept =
    default;

NetErrorCode ServerNetManager::Initialize(const NetConfig net_config,
                                          INetEventManager* event_manager) {
  if (event_manager == nullptr || net_config.io_work_thread_count_ <= 0 ||
      net_config.max_session_count_ <= 0 ||
      net_config.session_cfg_.max_recv_packet_size_ <= 0 ||
      net_config.session_cfg_.max_send_packet_size_ <= 0 ||
      net_config.session_cfg_.max_recv_buffer_size_ <= 0 ||
      net_config.session_cfg_.max_send_buffer_size_ <= 0 ||
      net_config.session_cfg_.max_send_count_per_1sec_ <= 0) {
    return NetErrorCode::kINVALID_CONFIG;
  }

  impl_->event_manager_ = event_manager;
  auto server = Server::Create(ToServerConfig(net_config), *impl_);
  if (!server) {
    return NetErrorCode::kINVALID_CONFIG;
  }
  impl_->server_.emplace(std::move(*server));
  return NetErrorCode::kNONE;
}

void ServerNetManager::Start() {
  if (impl_->server_) {
    static_cast<void>(impl_->server_->Start());
  }
}

void ServerNetManager::End() {
  if (impl_->server_) {
    static_cast<void>(impl_->server_->Stop(StopMode::kImmediate));
    impl_->server_.reset();
  }
}

NetErrorCode ServerNetManager::SendData(const std::int32_t session_index,
                                        const std::uint64_t session_uid,
                                        const std::uint32_t data_size,
                                        const char* data) {
  if (!impl_->server_ || data == nullptr) {
    return NetErrorCode::kSEND_INVALID_SESSION;
  }
  const auto bytes = std::span<const std::byte>(
      reinterpret_cast<const std::byte*>(data), data_size);
  switch (impl_->server_->Send(
      SessionId{.index = static_cast<std::uint32_t>(session_index),
                .generation = session_uid},
      bytes)) {
    case SendResult::kQueued:
      return NetErrorCode::kNONE;
    case SendResult::kStaleSession:
      return NetErrorCode::kSEND_INVALID_SESSION;
    case SendResult::kClosing:
      return NetErrorCode::kSEND_SESSION_NETWORK_DISABLE;
    case SendResult::kWouldBlock:
      return NetErrorCode::kSEND_THREAD_BUFFER_FULL;
    case SendResult::kMessageTooLarge:
    case SendResult::kUnavailable:
      return NetErrorCode::kSEND_IO_FAIL;
  }
  return NetErrorCode::kSEND_IO_FAIL;
}

void ServerNetManager::ForcedClose(const std::int32_t session_index,
                                   const std::uint64_t session_uid) {
  if (impl_->server_) {
    static_cast<void>(impl_->server_->RequestClose(
        SessionId{.index = static_cast<std::uint32_t>(session_index),
                  .generation = session_uid}));
  }
}

}  // namespace rionetlite
