#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace rionetlite {

/// @api DecodeError - length-prefix decode 실패 원인이다.
enum class DecodeError : std::uint8_t {
  kNone,
  kInvalidLength,
  kMessageTooLarge,
};

/// @api DecodeResult - 소비 바이트와 decode 오류를 반환한다.
struct DecodeResult final {
  std::size_t consumed_bytes = 0;
  std::size_t packet_count = 0;
  DecodeError error = DecodeError::kNone;
};

/// @api LengthPrefixedDecoder - big-endian length-prefix stream decoder다.
class LengthPrefixedDecoder final {
 public:
  static constexpr std::size_t kHeaderBytes = sizeof(std::uint32_t);

  explicit LengthPrefixedDecoder(std::uint32_t max_packet_bytes) noexcept
      : max_packet_bytes_(max_packet_bytes) {}

  template <typename PacketHandler>
  DecodeResult Decode(std::span<const std::byte> data,
                      PacketHandler&& handler) const {
    DecodeResult result;
    while (data.size() - result.consumed_bytes >= kHeaderBytes) {
      const auto header = data.subspan(result.consumed_bytes, kHeaderBytes);
      const std::uint32_t packet_size =
          (std::to_integer<std::uint32_t>(header[0]) << 24U) |
          (std::to_integer<std::uint32_t>(header[1]) << 16U) |
          (std::to_integer<std::uint32_t>(header[2]) << 8U) |
          std::to_integer<std::uint32_t>(header[3]);
      if (packet_size < kHeaderBytes) {
        result.error = DecodeError::kInvalidLength;
        return result;
      }
      if (packet_size > max_packet_bytes_) {
        result.error = DecodeError::kMessageTooLarge;
        return result;
      }
      if (packet_size > data.size() - result.consumed_bytes) {
        return result;
      }
      handler(data.subspan(result.consumed_bytes, packet_size));
      result.consumed_bytes += packet_size;
      ++result.packet_count;
    }
    return result;
  }

 private:
  std::uint32_t max_packet_bytes_;
};

}  // namespace rionetlite
