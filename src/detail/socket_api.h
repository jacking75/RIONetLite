#pragma once

#include <WinSock2.h>

#include <MSWSock.h>

namespace rionetlite::detail {

class SocketApi {
 public:
  virtual ~SocketApi() = default;

  virtual SOCKET CreateRegisteredSocket() noexcept = 0;
  virtual int SetExclusiveAddressUse(SOCKET socket) noexcept = 0;
  virtual int Bind(SOCKET socket, const SOCKADDR* address,
                   int address_length) noexcept = 0;
  virtual int Listen(SOCKET socket, int backlog) noexcept = 0;
};

class WindowsSocketApi final : public SocketApi {
 public:
  SOCKET CreateRegisteredSocket() noexcept override {
    return WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0,
                      WSA_FLAG_REGISTERED_IO);
  }

  int SetExclusiveAddressUse(const SOCKET socket) noexcept override {
    constexpr int enabled = 1;
    return setsockopt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                      reinterpret_cast<const char*>(&enabled), sizeof(enabled));
  }

  int Bind(const SOCKET socket, const SOCKADDR* address,
           const int address_length) noexcept override {
    return bind(socket, address, address_length);
  }

  int Listen(const SOCKET socket, const int backlog) noexcept override {
    return listen(socket, backlog);
  }
};

}  // namespace rionetlite::detail
