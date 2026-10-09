#pragma once

#include "../../src/detail/rio_api.h"
#include "../../src/detail/socket_api.h"
#include "../../src/detail/win32_resources.h"
#include "..\define.h"

#include <WinSock2.h>

#include <utility>

namespace rionetlite {
struct ServerNetManagerInit {
  static NetErrorCode Initialize(detail::WinsockRuntime& winsock,
                                 detail::UniqueSocket& listen_socket,
                                 detail::RioApi& rio_api,
                                 detail::SocketApi& socket_api,
                                 NetConfig net_cfg,
                                 std::uint32_t* native_error = nullptr) {
    if (net_cfg.port_number_ == 0 || net_cfg.io_work_thread_count_ <= 0 ||
        net_cfg.max_session_count_ <= 0 ||
        net_cfg.io_work_thread_count_ > net_cfg.max_session_count_ ||
        net_cfg.session_cfg_.max_try_1recv_buffer_size == 0 ||
        net_cfg.session_cfg_.max_recv_buffer_size_ <= 0 ||
        net_cfg.session_cfg_.max_send_buffer_size_ <= 0 ||
        net_cfg.session_cfg_.max_send_count_per_1sec_ <= 0) {
      return NetErrorCode::kINVALID_CONFIG;
    }

    if (const auto result = winsock.Start(); result != 0) {
      if (native_error != nullptr)
        *native_error = static_cast<std::uint32_t>(result);
      return NetErrorCode::kWSASTARTUP_FAIL;
    }

    if (auto result = ServerNetManagerInit::BindListenSocket(
            listen_socket, net_cfg.port_number_, socket_api, native_error);
        result != NetErrorCode::kNONE) {
      return result;
    }

    if (!rio_api.Initialize(listen_socket.get())) {
      if (native_error != nullptr) *native_error = WSAGetLastError();
      return NetErrorCode::kRIO_FUNCTION_INIT_FAIL;
    }

    if (SOCKET_ERROR == socket_api.Listen(listen_socket.get(), SOMAXCONN)) {
      if (native_error != nullptr) *native_error = WSAGetLastError();
      return NetErrorCode::kLISTEN_SOCKET_LISTEN_FAIL;
    }

    return NetErrorCode::kNONE;
  }

  static NetErrorCode BindListenSocket(detail::UniqueSocket& listen_socket,
                                       const UINT16 port,
                                       detail::SocketApi& socket_api,
                                       std::uint32_t* native_error = nullptr) {
    /// create TCP socket with RIO mode
    detail::UniqueSocket candidate(socket_api.CreateRegisteredSocket());
    if (!candidate) {
      if (native_error != nullptr) *native_error = WSAGetLastError();
      return NetErrorCode::kLISTEN_SOCKET_CREATE_FAIL;
    }

    if (socket_api.SetExclusiveAddressUse(candidate.get()) == SOCKET_ERROR) {
      if (native_error != nullptr) *native_error = WSAGetLastError();
      return NetErrorCode::kLISTEN_SOCKET_BIND_FAIL;
    }

    SOCKADDR_IN serveraddr{};
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (SOCKET_ERROR ==
        socket_api.Bind(candidate.get(),
                        reinterpret_cast<const SOCKADDR*>(&serveraddr),
                        sizeof(serveraddr))) {
      if (native_error != nullptr) *native_error = WSAGetLastError();
      return NetErrorCode::kLISTEN_SOCKET_BIND_FAIL;
    }

    listen_socket = std::move(candidate);

    return NetErrorCode::kNONE;
  }
};

}  // namespace rionetlite
