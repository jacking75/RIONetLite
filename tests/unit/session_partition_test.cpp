#include "detail/session_partition.h"

#include <gtest/gtest.h>

#include <cstdint>

namespace rionetlite::detail {
namespace {

TEST(SessionPartitionTest, AssignsEverySessionExactlyOnce) {
  constexpr std::uint32_t kSessions = 257;
  constexpr std::uint32_t kWorkers = 4;
  std::uint32_t next_index = 0;
  std::uint32_t total = 0;

  for (std::uint32_t worker = 0; worker < kWorkers; ++worker) {
    const auto partition = PartitionSessions(kSessions, kWorkers, worker);
    EXPECT_EQ(partition.first_index, next_index);
    EXPECT_LE(partition.count, 65U);
    EXPECT_GE(partition.count, 64U);
    next_index += partition.count;
    total += partition.count;
  }

  EXPECT_EQ(total, kSessions);
  EXPECT_EQ(next_index, kSessions);
}

TEST(SessionPartitionTest, RejectsInvalidWorkerCoordinates) {
  EXPECT_EQ(PartitionSessions(10, 0, 0).count, 0U);
  EXPECT_EQ(PartitionSessions(10, 2, 2).count, 0U);
}

}  // namespace
}  // namespace rionetlite::detail
