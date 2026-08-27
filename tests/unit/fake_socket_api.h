#pragma once

#include "detail/socket_api.h"

#include <cstdint>

namespace rionetlite::test {

class FakeSocketApi final : public detail::SocketApi {
 public:
  SOCKET CreateRegisteredSocket() noexcept override {
    ++create_calls;
    if (create_calls == create_fail_on_call) {
      WSASetLastError(WSAEMFILE);
      return INVALID_SOCKET;
    }
    return WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0,
                      WSA_FLAG_REGISTERED_IO);
  }

  int SetExclusiveAddressUse(SOCKET) noexcept override {
    ++set_option_calls;
    if (set_option_calls == set_option_fail_on_call) {
      WSASetLastError(WSAEINVAL);
      return SOCKET_ERROR;
    }
    return 0;
  }

  int Bind(SOCKET, const SOCKADDR*, int) noexcept override {
    ++bind_calls;
    if (bind_calls == bind_fail_on_call) {
      WSASetLastError(WSAEADDRINUSE);
      return SOCKET_ERROR;
    }
    return 0;
  }

  int Listen(SOCKET, int) noexcept override {
    ++listen_calls;
    if (listen_calls == listen_fail_on_call) {
      WSASetLastError(WSAEINVAL);
      return SOCKET_ERROR;
    }
    return 0;
  }

  std::uint32_t create_fail_on_call = 0;
  std::uint32_t set_option_fail_on_call = 0;
  std::uint32_t bind_fail_on_call = 0;
  std::uint32_t listen_fail_on_call = 0;
  std::uint32_t create_calls = 0;
  std::uint32_t set_option_calls = 0;
  std::uint32_t bind_calls = 0;
  std::uint32_t listen_calls = 0;
};

}  // namespace rionetlite::test
