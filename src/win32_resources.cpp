#include "detail/win32_resources.h"

#include "detail/rio_api.h"

#include <cassert>
#include <limits>
#include <utility>

namespace rionetlite::detail {

WinsockRuntime::WinsockRuntime(WinsockRuntime&& other) noexcept
    : active_(std::exchange(other.active_, false)) {}

WinsockRuntime& WinsockRuntime::operator=(WinsockRuntime&& other) noexcept {
  if (this != &other) {
    Reset();
    active_ = std::exchange(other.active_, false);
  }
  return *this;
}

WinsockRuntime::~WinsockRuntime() { Reset(); }

int WinsockRuntime::Start() noexcept {
  if (active_) {
    return 0;
  }
  WSADATA data{};
  const int result = WSAStartup(MAKEWORD(2, 2), &data);
  active_ = result == 0;
  return result;
}

void WinsockRuntime::Reset() noexcept {
  if (active_) {
    WSACleanup();
    active_ = false;
  }
}

UniqueSocket::UniqueSocket(UniqueSocket&& other) noexcept
    : socket_(other.Release()) {}

UniqueSocket& UniqueSocket::operator=(UniqueSocket&& other) noexcept {
  if (this != &other) {
    Reset(other.Release());
  }
  return *this;
}

UniqueSocket::~UniqueSocket() { Reset(); }

void UniqueSocket::Reset(const SOCKET socket) noexcept {
  if (socket_ != INVALID_SOCKET) {
    closesocket(socket_);
  }
  socket_ = socket;
}

SOCKET UniqueSocket::Release() noexcept {
  return std::exchange(socket_, INVALID_SOCKET);
}

UniqueHandle::UniqueHandle(UniqueHandle&& other) noexcept
    : handle_(other.Release()) {}

UniqueHandle& UniqueHandle::operator=(UniqueHandle&& other) noexcept {
  if (this != &other) {
    Reset(other.Release());
  }
  return *this;
}

UniqueHandle::~UniqueHandle() { Reset(); }

void UniqueHandle::Reset(const HANDLE handle) noexcept {
  if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(handle_);
  }
  handle_ = handle;
}

HANDLE UniqueHandle::Release() noexcept {
  return std::exchange(handle_, nullptr);
}

DoubleMappedBuffer::DoubleMappedBuffer(DoubleMappedBuffer&& other) noexcept
    : first_view_(std::exchange(other.first_view_, nullptr)),
      second_view_(std::exchange(other.second_view_, nullptr)),
      size_(std::exchange(other.size_, 0)) {}

DoubleMappedBuffer& DoubleMappedBuffer::operator=(
    DoubleMappedBuffer&& other) noexcept {
  if (this != &other) {
    Reset();
    first_view_ = std::exchange(other.first_view_, nullptr);
    second_view_ = std::exchange(other.second_view_, nullptr);
    size_ = std::exchange(other.size_, 0);
  }
  return *this;
}

DoubleMappedBuffer::~DoubleMappedBuffer() { Reset(); }

std::expected<DoubleMappedBuffer, DWORD> DoubleMappedBuffer::Create(
    const std::uint32_t size) noexcept {
  SYSTEM_INFO system_info{};
  GetSystemInfo(&system_info);
  if (size == 0 || size > std::numeric_limits<std::uint32_t>::max() / 2 ||
      size % system_info.dwPageSize != 0) {
    return std::unexpected(ERROR_INVALID_PARAMETER);
  }

  void* first_placeholder = VirtualAlloc2(
      nullptr, nullptr, static_cast<SIZE_T>(size) * 2,
      MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0);
  if (first_placeholder == nullptr) {
    return std::unexpected(GetLastError());
  }

// MEM_PRESERVE_PLACEHOLDER is the documented exception that requires the
// split range size even though MEM_RELEASE normally requires zero.
#pragma warning(suppress : 6333 28160)
  if (!VirtualFree(first_placeholder, size,
                   MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)) {
    const DWORD error = GetLastError();
    VirtualFree(first_placeholder, 0, MEM_RELEASE);
    return std::unexpected(error);
  }

  void* second_placeholder = static_cast<std::byte*>(first_placeholder) + size;
  UniqueHandle mapping(CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
                                          PAGE_READWRITE, 0, size, nullptr));
  if (!mapping) {
    const DWORD error = GetLastError();
    VirtualFree(first_placeholder, 0, MEM_RELEASE);
    VirtualFree(second_placeholder, 0, MEM_RELEASE);
    return std::unexpected(error);
  }

  void* first_view =
      MapViewOfFile3(mapping.get(), nullptr, first_placeholder, 0, size,
                     MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0);
  if (first_view == nullptr) {
    const DWORD error = GetLastError();
    VirtualFree(first_placeholder, 0, MEM_RELEASE);
    VirtualFree(second_placeholder, 0, MEM_RELEASE);
    return std::unexpected(error);
  }

  void* second_view =
      MapViewOfFile3(mapping.get(), nullptr, second_placeholder, 0, size,
                     MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0);
  if (second_view == nullptr) {
    const DWORD error = GetLastError();
    UnmapViewOfFile(first_view);
    VirtualFree(second_placeholder, 0, MEM_RELEASE);
    return std::unexpected(error);
  }

  DoubleMappedBuffer buffer;
  buffer.first_view_ = first_view;
  buffer.second_view_ = second_view;
  buffer.size_ = size;
  return buffer;
}

void DoubleMappedBuffer::Reset() noexcept {
  if (first_view_ != nullptr) {
    UnmapViewOfFile(first_view_);
  }
  if (second_view_ != nullptr) {
    UnmapViewOfFile(second_view_);
  }
  first_view_ = nullptr;
  second_view_ = nullptr;
  size_ = 0;
}

RegisteredRioBuffer::RegisteredRioBuffer(RegisteredRioBuffer&& other) noexcept
    : rio_(std::exchange(other.rio_, nullptr)),
      id_(std::exchange(other.id_, RIO_INVALID_BUFFERID)),
      outstanding_(other.outstanding_.exchange(0, std::memory_order_acq_rel)) {}

RegisteredRioBuffer& RegisteredRioBuffer::operator=(
    RegisteredRioBuffer&& other) noexcept {
  if (this != &other) {
    const bool reset = Reset();
    assert(reset);
    rio_ = std::exchange(other.rio_, nullptr);
    id_ = std::exchange(other.id_, RIO_INVALID_BUFFERID);
    outstanding_.store(
        other.outstanding_.exchange(0, std::memory_order_acq_rel),
        std::memory_order_release);
  }
  return *this;
}

RegisteredRioBuffer::~RegisteredRioBuffer() {
  const bool reset = Reset();
  assert(reset);
}

bool RegisteredRioBuffer::Register(RioApi& rio, char* data,
                                   const std::uint32_t size) noexcept {
  if (data == nullptr || size == 0 || id_ != RIO_INVALID_BUFFERID) {
    return false;
  }
  const RIO_BUFFERID id = rio.RegisterBuffer(data, size);
  if (id == RIO_INVALID_BUFFERID) {
    return false;
  }
  rio_ = &rio;
  id_ = id;
  outstanding_.store(0, std::memory_order_release);
  return true;
}

bool RegisteredRioBuffer::Reset() noexcept {
  if (id_ == RIO_INVALID_BUFFERID) {
    return true;
  }
  if (outstanding_.load(std::memory_order_acquire) != 0) {
    return false;
  }
  rio_->DeregisterBuffer(id_);
  id_ = RIO_INVALID_BUFFERID;
  rio_ = nullptr;
  return true;
}

void RegisteredRioBuffer::AddOutstanding() noexcept {
  outstanding_.fetch_add(1, std::memory_order_acq_rel);
}

void RegisteredRioBuffer::CompleteOutstanding() noexcept {
  const auto previous = outstanding_.fetch_sub(1, std::memory_order_acq_rel);
  assert(previous > 0);
}

void RegisteredRioBuffer::AbandonOutstanding() noexcept {
  outstanding_.store(0, std::memory_order_release);
}

UniqueRioCompletionQueue::UniqueRioCompletionQueue(
    UniqueRioCompletionQueue&& other) noexcept
    : rio_(std::exchange(other.rio_, nullptr)),
      queue_(std::exchange(other.queue_, RIO_INVALID_CQ)) {}

UniqueRioCompletionQueue& UniqueRioCompletionQueue::operator=(
    UniqueRioCompletionQueue&& other) noexcept {
  if (this != &other) {
    Reset();
    rio_ = std::exchange(other.rio_, nullptr);
    queue_ = std::exchange(other.queue_, RIO_INVALID_CQ);
  }
  return *this;
}

UniqueRioCompletionQueue::~UniqueRioCompletionQueue() { Reset(); }

void UniqueRioCompletionQueue::Reset() noexcept {
  if (queue_ != RIO_INVALID_CQ) {
    rio_->CloseCompletionQueue(queue_);
    queue_ = RIO_INVALID_CQ;
    rio_ = nullptr;
  }
}

}  // namespace rionetlite::detail
