#include "../../RIONetLite/impl/send_immediately.h"
#include "fake_rio_api.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace rionetlite {
namespace {

SessionNetConfig SendConfig() {
  SessionNetConfig config;
  config.max_send_packet_size_ = 1024;
  config.max_send_count_per_1sec_ = 1;
  config.max_send_buffer_size_ = 4096;
  return config;
}

NetErrorCode Post(SendImmediately& sender, const std::uint64_t uid,
                  const std::string& payload) {
  return sender.PostSend(uid, static_cast<std::uint32_t>(payload.size()),
                         payload.data());
}

TEST(RioRequestTest, RollsBackOutstandingWhenPostFails) {
  test::FakeRioApi rio;
  rio.send_succeeds = false;
  SendImmediately sender;
  ASSERT_TRUE(sender.Init(7, SendConfig(), rio));
  sender.OnConnect(rio.request_queue, 11);

  EXPECT_EQ(Post(sender, 11, "abc"), NetErrorCode::kSEND_IO_FAIL);
  EXPECT_EQ(sender.Outstanding(), 0U);
  EXPECT_EQ(rio.send_calls, 1U);
  sender.Dispose();
}

TEST(SendQueueTest, KeepsOnlyOneRequestInFlightPerSession) {
  test::FakeRioApi rio;
  SendImmediately sender;
  ASSERT_TRUE(sender.Init(3, SendConfig(), rio));
  sender.OnConnect(rio.request_queue, 5);

  ASSERT_EQ(Post(sender, 5, "aa"), NetErrorCode::kNONE);
  EXPECT_EQ(rio.send_calls, 1U);
  EXPECT_EQ(sender.Outstanding(), 1U);

  // 이미 요청이 진행 중이면 링에만 쌓이고 추가 RIOSend는 발생하지 않는다.
  ASSERT_EQ(Post(sender, 5, "bbb"), NetErrorCode::kNONE);
  ASSERT_EQ(Post(sender, 5, "cccc"), NetErrorCode::kNONE);
  EXPECT_EQ(rio.send_calls, 1U);
  EXPECT_EQ(sender.Outstanding(), 1U);

  auto* context = static_cast<RioIoContext*>(rio.last_request_context);
  ASSERT_EQ(rio.posted_buffers.size(), std::size_t{1});
  EXPECT_EQ(rio.posted_buffers.front().Length, 2U);

  // 완료 시 대기 중인 바이트를 하나의 요청으로 묶어 다시 게시한다.
  ASSERT_TRUE(sender.Completed(context, 2));
  EXPECT_EQ(rio.send_calls, 2U);
  ASSERT_EQ(rio.posted_buffers.size(), std::size_t{2});
  EXPECT_EQ(rio.posted_buffers.back().Length, 7U);
  EXPECT_TRUE(
      std::equal(std::begin("bbbcccc"), std::end("bbbcccc") - 1,
                 sender.GetBuffer() + rio.posted_buffers.back().Offset));

  ASSERT_TRUE(sender.Completed(context, 7));
  EXPECT_EQ(rio.send_calls, 2U);
  EXPECT_EQ(sender.Outstanding(), 0U);
  sender.Dispose();
}

TEST(SendQueueTest, RepostsOnlyTheRemainderAfterPartialCompletion) {
  test::FakeRioApi rio;
  SendImmediately sender;
  ASSERT_TRUE(sender.Init(3, SendConfig(), rio));
  sender.OnConnect(rio.request_queue, 5);

  ASSERT_EQ(Post(sender, 5, "rio!"), NetErrorCode::kNONE);
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

TEST(SendQueueTest, AppliesByteBackpressureWithoutOverwritingOutstandingBytes) {
  test::FakeRioApi rio;
  SendImmediately sender;
  ASSERT_TRUE(sender.Init(3, SendConfig(), rio));
  sender.OnConnect(rio.request_queue, 5);
  const std::string chunk(1024, 'x');
  const std::string first(1024, 'f');

  ASSERT_EQ(Post(sender, 5, first), NetErrorCode::kNONE);
  auto* context = static_cast<RioIoContext*>(rio.last_request_context);
  for (int index = 0; index < 3; ++index) {
    ASSERT_EQ(Post(sender, 5, chunk), NetErrorCode::kNONE) << index;
  }

  // 링이 가득 차면 슬롯이 아니라 바이트 기준으로 배압이 걸린다.
  EXPECT_EQ(Post(sender, 5, "!"), NetErrorCode::kSEND_THREAD_BUFFER_FULL);
  EXPECT_TRUE(std::equal(first.begin(), first.end(),
                         sender.GetBuffer() + context->Offset));

  ASSERT_TRUE(sender.Completed(context, 1024));
  EXPECT_EQ(Post(sender, 5, "!"), NetErrorCode::kNONE);
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

  EXPECT_EQ(Post(sender, 5, "new"), NetErrorCode::kSEND_INVALID_SESSION);
  EXPECT_EQ(Post(sender, 6, "new"), NetErrorCode::kNONE);
  sender.AbandonOutstanding();
  sender.Dispose();
}

TEST(SendQueueTest, WritesHeaderAndPayloadAsOneContiguousMessage) {
  test::FakeRioApi rio;
  SendImmediately sender;
  ASSERT_TRUE(sender.Init(3, SendConfig(), rio));
  sender.OnConnect(rio.request_queue, 5);
  const std::array<char, 4> header{'\0', '\0', '\0', '\x08'};
  const std::array<char, 4> payload{'p', 'i', 'n', 'g'};

  ASSERT_EQ(sender.PostSend(
                5, static_cast<std::uint32_t>(header.size()), header.data(),
                static_cast<std::uint32_t>(payload.size()), payload.data()),
            NetErrorCode::kNONE);
  ASSERT_EQ(rio.posted_buffers.size(), std::size_t{1});
  EXPECT_EQ(rio.posted_buffers.front().Length, 8U);
  const char* written = sender.GetBuffer() + rio.posted_buffers.front().Offset;
  EXPECT_TRUE(std::equal(header.begin(), header.end(), written));
  EXPECT_TRUE(std::equal(payload.begin(), payload.end(), written + 4));

  auto* context = static_cast<RioIoContext*>(rio.last_request_context);
  ASSERT_TRUE(sender.Completed(context, 8));
  EXPECT_EQ(sender.Outstanding(), 0U);
  sender.Dispose();
}

TEST(SendQueueTest, RejectsScatterMessageLargerThanTheConfiguredMaximum) {
  test::FakeRioApi rio;
  SendImmediately sender;
  ASSERT_TRUE(sender.Init(3, SendConfig(), rio));
  sender.OnConnect(rio.request_queue, 5);
  const std::string header(512, 'h');
  const std::string payload(513, 'p');

  EXPECT_EQ(sender.PostSend(
                5, static_cast<std::uint32_t>(header.size()), header.data(),
                static_cast<std::uint32_t>(payload.size()), payload.data()),
            NetErrorCode::kSEND_MESSAGE_TOO_LARGE);
  EXPECT_EQ(sender.Outstanding(), 0U);
  sender.Dispose();
}

TEST(SendQueueTest, RandomizedOperationsPreserveCapacityWithPrintedSeed) {
  constexpr std::uint32_t kSeed = 0x5A17C0DEU;
  SCOPED_TRACE(::testing::Message() << "seed=" << kSeed);
  std::mt19937 random(kSeed);
  test::FakeRioApi rio;
  rio.record_posted_buffers = false;
  SendImmediately sender;
  ASSERT_TRUE(sender.Init(3, SendConfig(), rio));
  sender.OnConnect(rio.request_queue, 5);
  const std::string bytes = "rio";
  RioIoContext* in_flight = nullptr;

  for (std::uint32_t iteration = 0; iteration < 10'000; ++iteration) {
    if ((random() & 1U) == 0) {
      const auto result = Post(sender, 5, bytes);
      EXPECT_TRUE(result == NetErrorCode::kNONE ||
                  result == NetErrorCode::kSEND_THREAD_BUFFER_FULL);
      if (result == NetErrorCode::kNONE && in_flight == nullptr) {
        in_flight = static_cast<RioIoContext*>(rio.last_request_context);
      }
    } else if (in_flight != nullptr) {
      ASSERT_TRUE(sender.Completed(in_flight, in_flight->Length));
      if (sender.Outstanding() == 0) {
        in_flight = nullptr;
      }
    }
    EXPECT_LE(sender.Outstanding(), 1U);
  }
  sender.AbandonOutstanding();
  sender.Dispose();
}

}  // namespace
}  // namespace rionetlite
