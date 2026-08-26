#pragma once

#include "../../src/detail/win32_resources.h"
#include "..\define.h"
#include "rio_context.h"

#include <WinSock2.h>

#include <utility>

namespace rionetlite {
struct ServerNetManagerInit {
  static NetErrorCode Initialize(detail::WinsockRuntime& winsock,
                                 detail::UniqueSocket& listen_socket,
                                 NetConfig net_cfg) {
    if (net_cfg.port_number_ == 0 || net_cfg.io_work_thread_count_ <= 0 ||
        net_cfg.max_session_count_ <= 0 ||
        net_cfg.io_work_thread_count_ > net_cfg.max_session_count_ ||
        net_cfg.session_cfg_.max_try_1recv_buffer_size == 0 ||
        net_cfg.session_cfg_.max_recv_buffer_size_ <= 0 ||
        net_cfg.session_cfg_.max_send_buffer_size_ <= 0 ||
        net_cfg.session_cfg_.max_send_count_per_1sec_ <= 0) {
      return NetErrorCode::kINVALID_CONFIG;
    }

    if (winsock.Start() != 0) {
      return NetErrorCode::kWSASTARTUP_FAIL;
    }

    if (auto result = ServerNetManagerInit::BindListenSocket(
            listen_socket, net_cfg.port_number_);
        result != NetErrorCode::kNONE) {
      return result;
    }

    if (RIOAPI::SetFunction(listen_socket.get()) == false) {
      return NetErrorCode::kRIO_FUNCTION_INIT_FAIL;
    }

    if (SOCKET_ERROR == listen(listen_socket.get(), SOMAXCONN)) {
      return NetErrorCode::kLISTEN_SOCKET_LISTEN_FAIL;
    }

    return NetErrorCode::kNONE;
  }

  static NetErrorCode BindListenSocket(detail::UniqueSocket& listen_socket,
                                       const UINT16 port) {
    /// create TCP socket with RIO mode
    detail::UniqueSocket candidate(WSASocketW(
        AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_REGISTERED_IO));
    if (!candidate) {
      return NetErrorCode::kLISTEN_SOCKET_CREATE_FAIL;
    }

    int opt = 1;
    if (setsockopt(candidate.get(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   reinterpret_cast<const char*>(&opt),
                   sizeof(opt)) == SOCKET_ERROR) {
      return NetErrorCode::kLISTEN_SOCKET_BIND_FAIL;
    }

    SOCKADDR_IN serveraddr;
    ZeroMemory(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (SOCKET_ERROR ==
        bind(candidate.get(), (SOCKADDR*)&serveraddr, sizeof(serveraddr))) {
      return NetErrorCode::kLISTEN_SOCKET_BIND_FAIL;
    }

    listen_socket = std::move(candidate);

    char logmsg[256] = {
        0,
    };
    sprintf_s(logmsg, "success Bind, Listen - Port: %d", port);
    LogFuncPtr((int)LogLevel::kINFO, logmsg);
    return NetErrorCode::kNONE;
  }
};

}  // namespace rionetlite
