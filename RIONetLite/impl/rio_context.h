#pragma once

#include <WinSock2.h>

#include <MSWSock.h>

#include <cstdint>

namespace rionetlite {
enum class IOType {
  kNONE,
  kSEND,
  kRECV,
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif

struct alignas(64) RioIoContext : public RIO_BUF {
  void Init(const int session_index, const IOType io_type,
            const std::uint32_t owner_worker = 0) noexcept {
    BufferId = RIO_INVALID_BUFFERID;
    Offset = 0;
    Length = 0;
    session_index_ = session_index;
    io_type_ = io_type;
    owner_worker_ = owner_worker;
    session_unique_Id_ = 0;
    request_number_ = 0;
    posted_length_ = 0;
    completed_length_ = 0;
  }

  bool BeginRequest(const UINT64 session_uid, const RIO_BUFFERID buffer_id,
                    const ULONG offset, const ULONG length,
                    const ULONG registered_size) noexcept {
    if (Length != 0 || length == 0 || offset > registered_size ||
        length > registered_size - offset) {
      return false;
    }
    session_unique_Id_ = session_uid;
    BufferId = buffer_id;
    Offset = offset;
    Length = length;
    posted_length_ = length;
    completed_length_ = 0;
    if (++request_number_ == 0) {
      ++request_number_;
    }
    return true;
  }

  bool CompleteSend(const ULONG transferred) noexcept {
    if (transferred == 0 || transferred > Length) {
      return false;
    }
    completed_length_ += transferred;
    Offset += transferred;
    Length -= transferred;
    return true;
  }

  bool CompleteReceive(const ULONG transferred) noexcept {
    if (transferred == 0 || transferred > Length) {
      return false;
    }
    completed_length_ = transferred;
    Length = 0;
    return true;
  }

  void Fail() noexcept {
    completed_length_ = posted_length_;
    Length = 0;
  }

  [[nodiscard]] bool IsIdle() const noexcept { return Length == 0; }

  int session_index_ = -1;
  IOType io_type_ = IOType::kNONE;
  UINT64 session_unique_Id_ = 0;
  std::uint32_t owner_worker_ = 0;
  std::uint64_t request_number_ = 0;
  ULONG posted_length_ = 0;
  ULONG completed_length_ = 0;
};

static_assert(alignof(RioIoContext) == 64);

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}  // namespace rionetlite
