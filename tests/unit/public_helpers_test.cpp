#include "rionetlite/packet_decoder.h"
#include "rionetlite/queued_event_adapter.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <vector>

namespace rionetlite {
namespace {

std::vector<std::byte> Packet(std::initializer_list<std::byte> payload) {
  const std::uint32_t size =
      static_cast<std::uint32_t>(payload.size() + sizeof(std::uint32_t));
  std::vector<std::byte> bytes{
      static_cast<std::byte>((size >> 24U) & 0xffU),
      static_cast<std::byte>((size >> 16U) & 0xffU),
      static_cast<std::byte>((size >> 8U) & 0xffU),
      static_cast<std::byte>(size & 0xffU),
  };
  bytes.insert(bytes.end(), payload);
  return bytes;
}

TEST(PacketDecoderTest, WaitsForFragmentsAndDecodesCoalescedPackets) {
  LengthPrefixedDecoder decoder(32);
  const auto first = Packet({std::byte{1}});
  const auto second = Packet({std::byte{2}, std::byte{3}});
  EXPECT_EQ(
      decoder.Decode(std::span(first).first(3), [](auto) {}).consumed_bytes,
      0U);

  std::vector<std::byte> combined = first;
  combined.insert(combined.end(), second.begin(), second.end());
  std::size_t callbacks = 0;
  const DecodeResult result = decoder.Decode(combined, [&](auto packet) {
    EXPECT_GE(packet.size(), LengthPrefixedDecoder::kHeaderBytes);
    ++callbacks;
  });
  EXPECT_EQ(result.error, DecodeError::kNone);
  EXPECT_EQ(result.consumed_bytes, combined.size());
  EXPECT_EQ(callbacks, 2U);
}

TEST(PacketDecoderTest, RejectsInvalidAndOversizedLengths) {
  LengthPrefixedDecoder decoder(8);
  const std::array invalid = {std::byte{0}, std::byte{0}, std::byte{0},
                              std::byte{3}};
  EXPECT_EQ(decoder.Decode(invalid, [](auto) {}).error,
            DecodeError::kInvalidLength);
  const std::array oversized = {std::byte{0}, std::byte{0}, std::byte{0},
                                std::byte{9}};
  EXPECT_EQ(decoder.Decode(oversized, [](auto) {}).error,
            DecodeError::kMessageTooLarge);
}

TEST(QueuedEventAdapterTest, PreservesPerShardOrderAndPayloadOwnership) {
  QueuedEventAdapter adapter(2, 4, 8, EventQueueFullPolicy::kRejectNewest);
  const SessionId session{.index = 3, .generation = 7};
  adapter.OnConnected(session);
  const std::array payload = {std::byte{1}, std::byte{2}};
  EXPECT_EQ(adapter.OnData(session, payload), payload.size());
  adapter.OnClosed(session, CloseReason::kPeerClosed);

  std::array<std::byte, 8> output{};
  QueuedEventHeader event;
  ASSERT_EQ(adapter.TryPop(1, output, event), PollEventResult::kEvent);
  EXPECT_EQ(event.type, QueuedEventType::kConnected);
  const std::uint64_t connect_sequence = event.sequence;
  ASSERT_EQ(adapter.TryPop(1, output, event), PollEventResult::kEvent);
  EXPECT_EQ(event.type, QueuedEventType::kData);
  EXPECT_GT(event.sequence, connect_sequence);
  EXPECT_EQ(output[1], std::byte{2});
  ASSERT_EQ(adapter.TryPop(1, output, event), PollEventResult::kEvent);
  EXPECT_EQ(event.type, QueuedEventType::kClosed);
}

TEST(QueuedEventAdapterTest, RejectsWithoutBlockingWhenQueueIsFull) {
  bool close_requested = false;
  QueuedEventAdapter adapter(1, 2, 4, EventQueueFullPolicy::kDisconnect,
                             [&](SessionId) { close_requested = true; });
  const SessionId session{0, 1};
  const std::array payload = {std::byte{1}};
  adapter.OnConnected(session);
  EXPECT_EQ(adapter.OnData(session, payload), 1U);
  EXPECT_EQ(adapter.OnData(session, payload), 0U);
  EXPECT_TRUE(close_requested);
  EXPECT_EQ(adapter.metrics().event_queue_rejections, 1U);
  EXPECT_EQ(adapter.metrics().event_queue_high_watermark, 2U);
}

TEST(QueuedEventAdapterTest, RejectsDataAfterCloseAndOldGeneration) {
  QueuedEventAdapter adapter(1, 8, 4, 1, EventQueueFullPolicy::kRejectNewest);
  const SessionId first{0, 1};
  const SessionId second{0, 2};
  const std::array payload{std::byte{7}};
  adapter.OnConnected(first);
  adapter.OnClosed(first, CloseReason::kPeerClosed);
  EXPECT_EQ(adapter.OnData(first, payload), 0U);
  adapter.OnConnected(second);
  EXPECT_EQ(adapter.OnData(first, payload), 0U);
  EXPECT_EQ(adapter.OnData(second, payload), 1U);
  EXPECT_EQ(adapter.metrics().event_queue_depth, 4U);
  EXPECT_EQ(adapter.metrics().event_queue_rejections, 2U);
}

TEST(QueuedEventAdapterTest, RejectsDataAfterCloseEvenWhenCloseQueueIsFull) {
  QueuedEventAdapter adapter(1, 1, 4, 1, EventQueueFullPolicy::kRejectNewest);
  const SessionId session{0, 1};
  const std::array payload{std::byte{7}};
  adapter.OnConnected(session);
  adapter.OnClosed(session, CloseReason::kPeerClosed);
  EXPECT_EQ(adapter.OnData(session, payload), 0U);
  // 종료 이벤트는 큐가 가득 차도 버리지 않으므로 rejection은 data 1건뿐이다.
  EXPECT_EQ(adapter.metrics().event_queue_rejections, 1U);
}

TEST(QueuedEventAdapterTest, DeliversCloseAfterTheRingDrainsWhenQueueWasFull) {
  QueuedEventAdapter adapter(1, 1, 4, 1, EventQueueFullPolicy::kRejectNewest);
  const SessionId session{0, 1};
  adapter.OnConnected(session);
  adapter.OnClosed(session, CloseReason::kPeerClosed);

  std::array<std::byte, 4> output{};
  QueuedEventHeader event;
  ASSERT_EQ(adapter.TryPop(0, output, event), PollEventResult::kEvent);
  EXPECT_EQ(event.type, QueuedEventType::kConnected);
  const std::uint64_t connect_sequence = event.sequence;

  // 보류된 close는 해당 세션의 이벤트를 모두 소비한 다음 순서를 지켜 나온다.
  ASSERT_EQ(adapter.TryPop(0, output, event), PollEventResult::kEvent);
  EXPECT_EQ(event.type, QueuedEventType::kClosed);
  EXPECT_EQ(event.close_reason, CloseReason::kPeerClosed);
  EXPECT_EQ(event.session, session);
  EXPECT_GT(event.sequence, connect_sequence);
  EXPECT_EQ(adapter.TryPop(0, output, event), PollEventResult::kEmpty);
}

TEST(QueuedEventAdapterTest, HoldsBackANewGenerationUntilTheParkedCloseIsSeen) {
  QueuedEventAdapter adapter(1, 1, 4, 1, EventQueueFullPolicy::kRejectNewest);
  const SessionId first{0, 1};
  const SessionId second{0, 2};
  adapter.OnConnected(first);
  adapter.OnClosed(first, CloseReason::kPeerClosed);

  std::array<std::byte, 4> output{};
  QueuedEventHeader event;
  ASSERT_EQ(adapter.TryPop(0, output, event), PollEventResult::kEvent);
  EXPECT_EQ(event.type, QueuedEventType::kConnected);

  // 이전 세대의 close가 아직 전달되지 않았으면 새 세대를 받지 않는다.
  adapter.OnConnected(second);
  ASSERT_EQ(adapter.TryPop(0, output, event), PollEventResult::kEvent);
  EXPECT_EQ(event.type, QueuedEventType::kClosed);
  EXPECT_EQ(event.session.generation, first.generation);

  adapter.OnConnected(second);
  ASSERT_EQ(adapter.TryPop(0, output, event), PollEventResult::kEvent);
  EXPECT_EQ(event.type, QueuedEventType::kConnected);
  EXPECT_EQ(event.session.generation, second.generation);
}

TEST(QueuedEventAdapterTest, ReportsPreallocatedMemoryBudget) {
  QueuedEventAdapter adapter(2, 8, 128, 32,
                             EventQueueFullPolicy::kRejectNewest);
  EXPECT_GT(adapter.EstimatedMemoryBytes(), 2U * 8U * 128U);
}

}  // namespace
}  // namespace rionetlite
