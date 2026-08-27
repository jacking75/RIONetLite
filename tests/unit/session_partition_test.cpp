#include "detail/session_partition.h"
#include "detail/processor_topology.h"

#include <gtest/gtest.h>

#include <array>
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

TEST(ProcessorTopologyTest, InterleavesNumaNodesAndPreservesGroupCoordinates) {
  constexpr std::array processors{
      ProcessorLocation{.group = 0, .number = 0, .numa_node = 0},
      ProcessorLocation{.group = 0, .number = 1, .numa_node = 0},
      ProcessorLocation{.group = 1, .number = 0, .numa_node = 1},
      ProcessorLocation{.group = 1, .number = 1, .numa_node = 1},
      ProcessorLocation{.group = 1, .number = 2, .numa_node = 1},
  };

  const auto ordered = OrderProcessorsForAffinity(processors);

  ASSERT_EQ(ordered.size(), processors.size());
  EXPECT_EQ(ordered[0], processors[0]);
  EXPECT_EQ(ordered[1], processors[2]);
  EXPECT_EQ(ordered[2], processors[1]);
  EXPECT_EQ(ordered[3], processors[3]);
  EXPECT_EQ(ordered[4], processors[4]);
}

}  // namespace
}  // namespace rionetlite::detail
