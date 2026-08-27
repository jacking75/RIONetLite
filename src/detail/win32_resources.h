#pragma once

#include <WinSock2.h>

#include <MSWSock.h>
#include <Windows.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>

namespace rionetlite::detail {

class RioApi;

class WinsockRuntime final {
 public:
  WinsockRuntime() = default;
  WinsockRuntime(const WinsockRuntime&) = delete;
  WinsockRuntime& operator=(const WinsockRuntime&) = delete;
  WinsockRuntime(WinsockRuntime&& other) noexcept;
  WinsockRuntime& operator=(WinsockRuntime&& other) noexcept;
  ~WinsockRuntime();

  int Start() noexcept;
  void Reset() noexcept;
  [[nodiscard]] bool active() const noexcept { return active_; }

 private:
  bool active_ = false;
};

class UniqueSocket final {
 public:
  UniqueSocket() = default;
  explicit UniqueSocket(SOCKET socket) noexcept : socket_(socket) {}
  UniqueSocket(const UniqueSocket&) = delete;
  UniqueSocket& operator=(const UniqueSocket&) = delete;
  UniqueSocket(UniqueSocket&& other) noexcept;
  UniqueSocket& operator=(UniqueSocket&& other) noexcept;
  ~UniqueSocket();

  void Reset(SOCKET socket = INVALID_SOCKET) noexcept;
  [[nodiscard]] SOCKET Release() noexcept;
  [[nodiscard]] SOCKET get() const noexcept { return socket_; }
  [[nodiscard]] explicit operator bool() const noexcept {
    return socket_ != INVALID_SOCKET;
  }

 private:
  SOCKET socket_ = INVALID_SOCKET;
};

class UniqueHandle final {
 public:
  UniqueHandle() = default;
  explicit UniqueHandle(HANDLE handle) noexcept : handle_(handle) {}
  UniqueHandle(const UniqueHandle&) = delete;
  UniqueHandle& operator=(const UniqueHandle&) = delete;
  UniqueHandle(UniqueHandle&& other) noexcept;
  UniqueHandle& operator=(UniqueHandle&& other) noexcept;
  ~UniqueHandle();

  void Reset(HANDLE handle = nullptr) noexcept;
  [[nodiscard]] HANDLE Release() noexcept;
  [[nodiscard]] HANDLE get() const noexcept { return handle_; }
  [[nodiscard]] explicit operator bool() const noexcept {
    return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
  }

 private:
  HANDLE handle_ = nullptr;
};

class DoubleMappedBuffer final {
 public:
  DoubleMappedBuffer() = default;
  DoubleMappedBuffer(const DoubleMappedBuffer&) = delete;
  DoubleMappedBuffer& operator=(const DoubleMappedBuffer&) = delete;
  DoubleMappedBuffer(DoubleMappedBuffer&& other) noexcept;
  DoubleMappedBuffer& operator=(DoubleMappedBuffer&& other) noexcept;
  ~DoubleMappedBuffer();

  static std::expected<DoubleMappedBuffer, DWORD> Create(
      std::uint32_t size) noexcept;

  void Reset() noexcept;
  [[nodiscard]] char* data() const noexcept {
    return static_cast<char*>(first_view_);
  }
  [[nodiscard]] std::uint32_t size() const noexcept { return size_; }
  [[nodiscard]] std::uint32_t mapped_size() const noexcept { return size_ * 2; }
  [[nodiscard]] explicit operator bool() const noexcept {
    return first_view_ != nullptr;
  }

 private:
  void* first_view_ = nullptr;
  void* second_view_ = nullptr;
  std::uint32_t size_ = 0;
};

class RegisteredRioBuffer final {
 public:
  RegisteredRioBuffer() = default;
  RegisteredRioBuffer(const RegisteredRioBuffer&) = delete;
  RegisteredRioBuffer& operator=(const RegisteredRioBuffer&) = delete;
  RegisteredRioBuffer(RegisteredRioBuffer&& other) noexcept;
  RegisteredRioBuffer& operator=(RegisteredRioBuffer&& other) noexcept;
  ~RegisteredRioBuffer();

  bool Register(RioApi& rio, char* data, std::uint32_t size) noexcept;
  bool Reset() noexcept;
  void AddOutstanding() noexcept;
  void CompleteOutstanding() noexcept;
  void AbandonOutstanding() noexcept;

  [[nodiscard]] RIO_BUFFERID get() const noexcept { return id_; }
  [[nodiscard]] std::uint32_t outstanding() const noexcept {
    return outstanding_.load(std::memory_order_acquire);
  }
  [[nodiscard]] explicit operator bool() const noexcept {
    return id_ != RIO_INVALID_BUFFERID;
  }

 private:
  RioApi* rio_ = nullptr;
  RIO_BUFFERID id_ = RIO_INVALID_BUFFERID;
  std::atomic<std::uint32_t> outstanding_{0};
};

class UniqueRioCompletionQueue final {
 public:
  UniqueRioCompletionQueue() = default;
  UniqueRioCompletionQueue(RioApi& rio, RIO_CQ queue) noexcept
      : rio_(&rio), queue_(queue) {}
  UniqueRioCompletionQueue(const UniqueRioCompletionQueue&) = delete;
  UniqueRioCompletionQueue& operator=(const UniqueRioCompletionQueue&) = delete;
  UniqueRioCompletionQueue(UniqueRioCompletionQueue&& other) noexcept;
  UniqueRioCompletionQueue& operator=(
      UniqueRioCompletionQueue&& other) noexcept;
  ~UniqueRioCompletionQueue();

  void Reset() noexcept;
  [[nodiscard]] RIO_CQ get() const noexcept { return queue_; }
  [[nodiscard]] explicit operator bool() const noexcept {
    return queue_ != RIO_INVALID_CQ;
  }

 private:
  RioApi* rio_ = nullptr;
  RIO_CQ queue_ = RIO_INVALID_CQ;
};

}  // namespace rionetlite::detail
