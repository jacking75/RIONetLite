#pragma once

#include "../../src/detail/win32_resources.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <tuple>
#include <utility>

namespace rionetlite {
// DoubleMappedBuffer의 두 연속 view를 사용해 wrap 구간도 하나의 RIO_BUF로
// 게시한다. 실제 backing storage의 크기는 ring_buffer_size_다.
class RingBufferVA2 {
 public:
  RingBufferVA2() = default;

  ~RingBufferVA2() = default;

  char* GetBuffer() { return ring_buffer_.data(); }

  std::tuple<int, std::string> Create(const std::uint32_t ring_buffer_size) {
    auto buffer = detail::DoubleMappedBuffer::Create(ring_buffer_size);
    if (!buffer) {
      return {static_cast<int>(buffer.error()),
              "DoubleMappedBuffer::Create failed"};
    }

    ring_buffer_ = std::move(*buffer);
    ring_buffer_size_ = ring_buffer_size;

    Clear();

    return {0, {}};
  }

  // VirtualAlloc2의 링버퍼를 사용하고 있어서 주소로는 2배까지 사용 가능하다.
  std::uint32_t RegisteredSpanSize() const { return ring_buffer_size_ * 2U; }

  // 커널이 아직 참조할 수 있는 매핑을 해제하지 않고 소유권만 포기한다.
  void ReleaseStorage() noexcept {
    ring_buffer_.Release();
    ring_buffer_size_ = 0;
    Clear();
  }

  void Clear() {
    write_pos_ = 0;
    read_pos_ = 0;
    used_bytes_ = 0;
  }

  std::tuple<std::uint32_t, char*> Read() {
    char* pData = nullptr;
    std::uint32_t readAbleSize = 0;

    if (used_bytes_ != 0) {
      readAbleSize = used_bytes_;
      pData = &ring_buffer_.data()[read_pos_];
    }

    return {readAbleSize, pData};
  }

  void IncremantReadPos(const std::uint32_t pos) {
    if (pos > used_bytes_) {
      return;
    }
    read_pos_ += pos;

    if (read_pos_ >= ring_buffer_size_) {
      read_pos_ -= ring_buffer_size_;
    }
    used_bytes_ -= pos;
  }

  char* Write(const std::uint32_t size, const char* p_data) {
    if (CheckRemainingSpace(size) == false) {
      return nullptr;
    }

    int oldPos = write_pos_;
    std::memcpy(&ring_buffer_.data()[write_pos_], p_data, size);
    write_pos_ += size;

    if (write_pos_ >= ring_buffer_size_) {
      write_pos_ -= ring_buffer_size_;
    }
    used_bytes_ += size;

    return &ring_buffer_.data()[oldPos];
  }

  bool Write(const std::uint32_t size) {
    if (CheckRemainingSpace(size) == false) {
      return false;
    }

    write_pos_ += size;

    if (write_pos_ >= ring_buffer_size_) {
      write_pos_ -= ring_buffer_size_;
    }
    used_bytes_ += size;

    return true;
  }

  std::uint32_t GetWriteablePos() const { return write_pos_; }

  std::uint32_t GetReadPos() const { return read_pos_; }

  std::uint32_t GetUsedSize() const { return used_bytes_; }

  std::uint32_t GetWritableSize() const {
    return ring_buffer_size_ - used_bytes_;
  }

 private:
  bool CheckRemainingSpace(const std::uint32_t desired_size) {
    return desired_size <= ring_buffer_size_ - used_bytes_;
  }

  std::uint32_t ring_buffer_size_ = 0;
  detail::DoubleMappedBuffer ring_buffer_;
  std::uint32_t write_pos_ = 0;
  std::uint32_t read_pos_ = 0;
  std::uint32_t used_bytes_ = 0;
};

}  // namespace rionetlite
