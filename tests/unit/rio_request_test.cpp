#include "../../RIONetLite/impl/send_immediately.h"
#include "fake_rio_api.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>

namespace rionetlite {
namespace {

SessionNetConfig SendConfig() {
  SessionNetConfig config;
  config.max_send_packet_size_ = 1024;
  config.max_send_count_per_1sec_ = 2;
  config.max_send_buffer_size_ = 4096;
  return config;
}

TEST(RioRequestTest, RollsBackOutstandingWhenPostFails) {
  test::FakeRioApi rio;
  rio.send_succeeds = false;
  SendImmediately sender;
  ASSERT_TRUE(sender.Init(7, SendConfig(), rio));
  sender.OnConnect(rio.request_queue, 11);
  const std::array data{'a', 'b', 'c'};

  EXPECT_EQ(
      sender.PostSend(11, static_cast<std::uint32_t>(data.size()), data.data()),
      NetErrorCode::kSEND_IO_FAIL);
  EXPECT_EQ(sender.Outstanding(), 0U);
  EXPECT_EQ(rio.send_calls, 1U);
  sender.Dispose();
}

TEST(SendQueueTest, RepostsOnlyTheRemainderAfterPartialCompletion) {
  test::FakeRioApi rio;
  SendImmediately sender;
  ASSERT_TRUE(sender.Init(3, SendConfig(), rio));
  sender.OnConnect(rio.request_queue, 5);
  const std::array data{'r', 'i', 'o', '!'};

  ASSERT_EQ(
      sender.PostSend(5, static_cast<std::uint32_t>(data.size()), data.data()),
      NetErrorCode::kNONE);
  ASSERT_EQ(sender.Outstanding(), 1U);
  ASSERT_EQ(rio.posted_buffers.size(), std::size_t{1});
  auto* context = static_cast<RioIoContext*>(rio.last_request_context);
  const auto original_offset = rio.posted_buffers.front().Offset;

  EXPECT_TRUE(sender.Completed(context, 2));
  ASSERT_EQ(rio.posted_buffers.size(), std::size_t{2});
  EXPECT_EQ(rio.posted_buffers.back().Offset, original_offset + 2);
  EXPECT_EQ(rio.posted_buffers.back().Length, 2U);
  EXPECT_EQ(sender.Outstanding(), 1U);

  EXPECT_TRUE(sender.Completed(context, 2));
  EXPECT_EQ(sender.Outstanding(), 0U);
  sender.Dispose();
}

TEST(SendQueueTest, AppliesBackpressureWithoutOverwritingOutstandingBytes) {
  test::FakeRioApi rio;
  SendImmediately sender;
  ASSERT_TRUE(sender.Init(3, SendConfig(), rio));
  sender.OnConnect(rio.request_queue, 5);
  const std::array first{'f', 'i', 'r', 's', 't'};
  const std::array second{'s', 'e', 'c', 'o', 'n', 'd'};
  const std::array third{'t', 'h', 'i', 'r', 'd'};

  ASSERT_EQ(sender.PostSend(5, static_cast<std::uint32_t>(first.size()),
                            first.data()),
            NetErrorCode::kNONE);
  auto* first_context = static_cast<RioIoContext*>(rio.last_request_context);
  ASSERT_EQ(sender.PostSend(5, static_cast<std::uint32_t>(second.size()),
                            second.data()),
            NetErrorCode::kNONE);
  auto* second_context = static_cast<RioIoContext*>(rio.last_request_context);
  ASSERT_NE(first_context, second_context);
  EXPECT_EQ(sender.PostSend(5, static_cast<std::uint32_t>(third.size()),
                            third.data()),
            NetErrorCode::kSEND_THREAD_BUFFER_FULL);
  EXPECT_TRUE(std::equal(second.begin(), second.end(),
                         sender.GetBuffer() + second_context->Offset));

  ASSERT_TRUE(sender.Completed(first_context,
                               static_cast<std::uint32_t>(first.size())));
  ASSERT_EQ(sender.PostSend(5, static_cast<std::uint32_t>(third.size()),
                            third.data()),
            NetErrorCode::kNONE);
  EXPECT_TRUE(std::equal(second.begin(), second.end(),
                         sender.GetBuffer() + second_context->Offset));
  sender.AbandonOutstanding();
  sender.Dispose();
}

TEST(SendQueueTest, RejectsOldGenerationAfterSlotReuseInsideQueueLock) {
  test::FakeRioApi rio;
  SendImmediately sender;
  ASSERT_TRUE(sender.Init(3, SendConfig(), rio));
  sender.OnConnect(rio.request_queue, 5);
  sender.BeginClose();
  sender.OnConnect(rio.request_queue, 6);
  const std::array data{'n', 'e', 'w'};

  EXPECT_EQ(
      sender.PostSend(5, static_cast<std::uint32_t>(data.size()), data.data()),
      NetErrorCode::kSEND_INVALID_SESSION);
  EXPECT_EQ(
      sender.PostSend(6, static_cast<std::uint32_t>(data.size()), data.data()),
      NetErrorCode::kNONE);
  sender.AbandonOutstanding();
  sender.Dispose();
}

TEST(SendQueueTest, RandomizedOperationsPreserveCapacityWithPrintedSeed) {
  constexpr std::uint32_t kSeed = 0x5A17C0DEU;
  SCOPED_TRACE(::testing::Message() << "seed=" << kSeed);
  std::mt19937 random(kSeed);
  test::FakeRioApi rio;
  rio.record_posted_buffers = false;
  auto config = SendConfig();
  config.max_send_count_per_1sec_ = 8;
  SendImmediately sender;
  ASSERT_TRUE(sender.Init(3, config, rio));
  sender.OnConnect(rio.request_queue, 5);
  std::array<RioIoContext*, 8> active{};
  const std::array bytes{'r', 'i', 'o'};

  for (std::uint32_t iteration = 0; iteration < 10'000; ++iteration) {
    if ((random() & 1U) == 0) {
      const auto result = sender.PostSend(
          5, static_cast<std::uint32_t>(bytes.size()), bytes.data());
      EXPECT_TRUE(result == NetErrorCode::kNONE ||
                  result == NetErrorCode::kSEND_THREAD_BUFFER_FULL);
      if (result == NetErrorCode::kNONE) {
        auto* context = static_cast<RioIoContext*>(rio.last_request_context);
        auto slot = std::find(active.begin(), active.end(), context);
        if (slot == active.end()) {
          slot = std::find(active.begin(), active.end(), nullptr);
        }
        ASSERT_NE(slot, active.end());
        *slot = context;
      }
    } else {
      const auto slot =
          std::find_if(active.begin(), active.end(),
                       [](auto* value) { return value != nullptr; });
      if (slot != active.end()) {
        ASSERT_TRUE(sender.Completed(*slot, (*slot)->Length));
        *slot = nullptr;
      }
    }
    EXPECT_LE(sender.Outstanding(), active.size());
  }
  sender.AbandonOutstanding();
  sender.Dispose();
}

}  // namespace
}  // namespace rionetlite
