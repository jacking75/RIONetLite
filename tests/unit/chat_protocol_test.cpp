#include "../../samples/chat_server/chat_protocol.h"

#include <gtest/gtest.h>

#include <charconv>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace rionetlite::chat {
namespace {

std::vector<std::byte> FromHex(const std::string& text) {
  std::vector<std::byte> bytes;
  if (text == "-") return bytes;
  for (std::size_t offset = 0; offset + 1 < text.size(); offset += 2) {
    unsigned int value = 0;
    const auto result = std::from_chars(text.data() + offset,
                                        text.data() + offset + 2, value, 16);
    if (result.ec != std::errc{}) return {};
    bytes.push_back(static_cast<std::byte>(value));
  }
  return bytes;
}

TEST(ChatProtocolTest, MatchesSharedCppAndCsharpGoldenFixtures) {
  const auto path = std::filesystem::path(__FILE__).parent_path() /
                    "../../docs/protocol/chat-fixtures.tsv";
  std::ifstream stream(path);
  ASSERT_TRUE(stream.is_open()) << path;
  std::string line;
  std::size_t count = 0;
  while (std::getline(stream, line)) {
    if (line.empty() || line.front() == '#') continue;
    if (line.back() == '\r') line.pop_back();
    std::istringstream row(line);
    std::string name, type_text, payload_text, wire_text;
    ASSERT_TRUE(std::getline(row, name, '\t'));
    ASSERT_TRUE(std::getline(row, type_text, '\t'));
    ASSERT_TRUE(std::getline(row, payload_text, '\t'));
    ASSERT_TRUE(std::getline(row, wire_text, '\t'));
    SCOPED_TRACE(name);
    const auto type = static_cast<std::uint16_t>(std::stoul(type_text));
    const auto payload = FromHex(payload_text);
    const auto expected = FromHex(wire_text);
    const auto header = EncodeHeader(type, payload.size());
    std::vector<std::byte> actual(header.begin(), header.end());
    actual.insert(actual.end(), payload.begin(), payload.end());
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(ReadU32(header.data()), expected.size());
    EXPECT_EQ(ReadU16(header.data() + 4), kProtocolVersion);
    EXPECT_EQ(ReadU16(header.data() + 6), type);
    ++count;
  }
  EXPECT_EQ(count, 8U);
}

}  // namespace
}  // namespace rionetlite::chat
