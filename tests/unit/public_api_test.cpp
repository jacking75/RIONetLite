#include "rionetlite/config.h"
#include "rionetlite/error.h"
#include "rionetlite/event_handler.h"
#include "rionetlite/metrics.h"
#include "rionetlite/packet_decoder.h"
#include "rionetlite/queued_event_adapter.h"
#include "rionetlite/server.h"
#include "rionetlite/version.h"

#include "../../RIONetLite/define.h"

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
  EXPECT_EQ(ToString(CloseReason::kIdleTimeout), "idle_timeout");
}

TEST(PublicApiTest, LegacyDisconnectReasonStringsMatchEveryValue) {
  EXPECT_STREQ(DisconnectReasonStr[static_cast<int>(DisconnectReason::kNONE)],
               "none");
  EXPECT_STREQ(DisconnectReasonStr[static_cast<int>(
                   DisconnectReason::kACCEPT_LOOP_FAIL_ON_CONNECT)],
               "accept_loop_fail_on_connect");
  EXPECT_STREQ(DisconnectReasonStr[static_cast<int>(
                   DisconnectReason::kSEND_THREAD_REAL_SEND_ERROR)],
               "send_thread_real_send_error");
  EXPECT_STREQ(DisconnectReasonStr[static_cast<int>(
                   DisconnectReason::kFORCES_CLOSE_SEND_ERROR)],
               "forces_close_send_error");
  EXPECT_STREQ(DisconnectReasonStr[static_cast<int>(
                   DisconnectReason::kRECV_BUFFER_FULL)],
               "recv_buffer_full");
  EXPECT_STREQ(
      DisconnectReasonStr[static_cast<int>(DisconnectReason::kPROTOCOL_ERROR)],
      "protocol_error");
  EXPECT_STREQ(
      DisconnectReasonStr[static_cast<int>(DisconnectReason::kOVERLOADED)],
      "overloaded");
  EXPECT_STREQ(
      DisconnectReasonStr[static_cast<int>(DisconnectReason::kIDLE_TIMEOUT)],
      "idle_timeout");
}

TEST(PublicApiTest, PublicHeadersExposeThePackageVersion) {
  EXPECT_STREQ(RIONETLITE_VERSION_STRING, "0.3.0");
}

}  // namespace
}  // namespace rionetlite
