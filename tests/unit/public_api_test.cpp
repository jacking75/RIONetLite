#include "rionetlite/server.h"

#include <gtest/gtest.h>

#include <type_traits>

namespace rionetlite {
namespace {

TEST(PublicApiTest, ServerIsMoveOnly) {
  static_assert(!std::is_copy_constructible_v<Server>);
  static_assert(std::is_move_constructible_v<Server>);
  SUCCEED();
}

TEST(PublicApiTest, EnumStringsAreStable) {
  EXPECT_EQ(ToString(SendResult::kWouldBlock), "would_block");
  EXPECT_EQ(ToString(CloseReason::kPeerClosed), "peer_closed");
}

}  // namespace
}  // namespace rionetlite
