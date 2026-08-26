#include "rionetlite/config.h"

#include <gtest/gtest.h>

#include <limits>

namespace rionetlite {
namespace {

ServerConfig ValidConfig() {
  ServerConfig config;
  config.port = 32452;
  config.io_worker_count = 4;
  config.max_sessions = 257;
  config.max_receive_chunk_bytes = 4096;
  config.receive_buffer_bytes = 64 * 1024;
  config.max_message_bytes = 16 * 1024;
  config.send_buffer_bytes = 64 * 1024;
  config.max_outstanding_sends_per_session = 8;
  config.completion_batch_size = 64;
  return config;
}

TEST(ConfigValidationTest, AcceptsNonDivisibleSessionCount) {
  auto validated = ValidateServerConfig(ValidConfig());
  ASSERT_TRUE(validated.has_value());
  EXPECT_EQ(validated->max_sessions_per_worker, 65U);
  EXPECT_EQ(validated->completion_queue_capacity, 585U);
}

TEST(ConfigValidationTest, RejectsZeroWorkerCountBeforeDivision) {
  auto config = ValidConfig();
  config.io_worker_count = 0;
  auto validated = ValidateServerConfig(config);
  ASSERT_FALSE(validated.has_value());
  EXPECT_EQ(validated.error().field, "io_worker_count");
}

TEST(ConfigValidationTest, RejectsUnalignedRegisteredBuffer) {
  auto config = ValidConfig();
  config.receive_buffer_bytes = 4097;
  auto validated = ValidateServerConfig(config);
  ASSERT_FALSE(validated.has_value());
  EXPECT_EQ(validated.error().field, "receive_buffer_bytes");
}

TEST(ConfigValidationTest, RejectsMessageLargerThanSendBuffer) {
  auto config = ValidConfig();
  config.max_message_bytes = config.send_buffer_bytes + 1;
  auto validated = ValidateServerConfig(config);
  ASSERT_FALSE(validated.has_value());
  EXPECT_EQ(validated.error().field, "max_message_bytes");
}

}  // namespace
}  // namespace rionetlite
