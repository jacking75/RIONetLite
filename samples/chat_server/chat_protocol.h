#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace rionetlite::chat {

inline constexpr std::uint16_t kProtocolVersion = 1;
inline constexpr std::uint32_t kMaxPacketBytes = 1024;
inline constexpr std::uint32_t kHeaderBytes = 8;
using PacketHeader = std::array<std::byte, kHeaderBytes>;

inline std::uint16_t ReadU16(const std::byte* data) {
  return (static_cast<std::uint16_t>(data[0]) << 8U) |
         static_cast<std::uint16_t>(data[1]);
}

inline std::uint32_t ReadU32(const std::byte* data) {
  return (static_cast<std::uint32_t>(data[0]) << 24U) |
         (static_cast<std::uint32_t>(data[1]) << 16U) |
         (static_cast<std::uint32_t>(data[2]) << 8U) |
         static_cast<std::uint32_t>(data[3]);
}

// Header length includes all eight header bytes. All integer fields use BE.
inline PacketHeader EncodeHeader(const std::uint16_t type,
                                 const std::size_t payload_bytes) {
  const auto length = static_cast<std::uint32_t>(kHeaderBytes + payload_bytes);
  return PacketHeader{
      static_cast<std::byte>((length >> 24U) & 0xffU),
      static_cast<std::byte>((length >> 16U) & 0xffU),
      static_cast<std::byte>((length >> 8U) & 0xffU),
      static_cast<std::byte>(length & 0xffU),
      static_cast<std::byte>((kProtocolVersion >> 8U) & 0xffU),
      static_cast<std::byte>(kProtocolVersion & 0xffU),
      static_cast<std::byte>((type >> 8U) & 0xffU),
      static_cast<std::byte>(type & 0xffU),
  };
}

}  // namespace rionetlite::chat
