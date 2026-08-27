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
  config.send_buffer_bytes = 128 * 1024;
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

TEST(ConfigValidationTest, RejectsWorkerCountAboveImplementationLimit) {
  auto config = ValidConfig();
  config.io_worker_count = 257;
  config.max_sessions = 257;
  const auto validated = ValidateServerConfig(config);
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

TEST(ConfigValidationTest, ReportsSeparateRegisteredAndVirtualMemoryBudgets) {
  ServerConfig config = ValidConfig();
  config.max_sessions = 4;
  const auto result = ValidateServerConfig(config);
  ASSERT_TRUE(result.has_value());
  const std::uint64_t registered_per_session =
      static_cast<std::uint64_t>(config.receive_buffer_bytes) +
      config.send_buffer_bytes;
  EXPECT_EQ(result->estimated_registered_bytes,
            registered_per_session * config.max_sessions * 2);
  EXPECT_EQ(result->estimated_reserved_virtual_bytes,
            result->estimated_registered_bytes);
  EXPECT_EQ(result->estimated_committed_bytes,
            registered_per_session * config.max_sessions);
  EXPECT_GT(result->estimated_control_bytes, 0U);
}

TEST(ConfigValidationTest, PreservesMetricsOptOut) {
  auto config = ValidConfig();
  config.enable_metrics = false;
  const auto result = ValidateServerConfig(config);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->value.enable_metrics);
}

TEST(ConfigValidationTest, RejectsImpossibleAggregateBudgetBeforeAllocation) {
  ServerConfig config;
  config.port = 32452;
  config.io_worker_count = 256;
  config.max_sessions =
      static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
  config.receive_buffer_bytes = 4096;
  config.send_buffer_bytes = 1024U * 1024U * 1024U;
  config.max_receive_chunk_bytes = 1;
  config.max_message_bytes = 1;
  config.max_outstanding_sends_per_session = 1024U * 1024U * 1024U;

  const auto result = ValidateServerConfig(config);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().field, "max_outstanding_sends_per_session");
}

}  // namespace
}  // namespace rionetlite
