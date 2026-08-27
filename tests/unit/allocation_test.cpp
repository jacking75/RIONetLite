#include "../../RIONetLite/impl/send_immediately.h"
#include "fake_rio_api.h"
#include "rionetlite/queued_event_adapter.h"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#ifdef _DEBUG
#include <crtdbg.h>
#endif

namespace rionetlite::detail {
namespace {

#ifdef _DEBUG
std::atomic<bool> g_count_allocations{false};
std::atomic<std::uint64_t> g_allocation_count{0};

int __cdecl AllocationHook(const int allocation_type, void*, std::size_t, int,
                           long, const unsigned char*, int) {
  if (allocation_type == _HOOK_ALLOC &&
      g_count_allocations.load(std::memory_order_relaxed)) {
    g_allocation_count.fetch_add(1, std::memory_order_relaxed);
  }
  return 1;
}
#endif

TEST(SteadyStateAllocationTest, CorePacketQueuesAllocateOnlyDuringSetup) {
#ifdef _DEBUG
  test::FakeRioApi rio;
  rio.record_posted_buffers = false;
  SessionNetConfig send_config;
  send_config.max_send_packet_size_ = 64;
  send_config.max_send_count_per_1sec_ = 2;
  send_config.max_send_buffer_size_ = 4096;
  SendImmediately sender;
  ASSERT_TRUE(sender.Init(0, send_config, rio));
  sender.OnConnect(rio.request_queue, 1);
  QueuedEventAdapter adapter(1, 2, 64, 1, EventQueueFullPolicy::kRejectNewest);
  const SessionId session{.index = 0, .generation = 1};
  std::array<std::byte, 16> bytes{};
  std::array<std::byte, 64> destination{};
  QueuedEventHeader header;
  adapter.OnConnected(session);
  ASSERT_EQ(adapter.TryPop(0, destination, header), PollEventResult::kEvent);

  const auto previous_hook = _CrtSetAllocHook(&AllocationHook);
  g_allocation_count.store(0, std::memory_order_relaxed);
  g_count_allocations.store(true, std::memory_order_release);
  bool operations_succeeded = true;
  for (std::uint32_t iteration = 0; iteration < 10'000; ++iteration) {
    const auto send_result = sender.PostSend(
        session.generation, static_cast<std::uint32_t>(bytes.size()),
        reinterpret_cast<const char*>(bytes.data()));
    auto* context = static_cast<RioIoContext*>(rio.last_request_context);
    operations_succeeded =
        operations_succeeded && send_result == NetErrorCode::kNONE &&
        context != nullptr &&
        sender.Completed(context, static_cast<std::uint32_t>(bytes.size()));
    adapter.OnData(session, bytes);
    operations_succeeded =
        operations_succeeded &&
        adapter.TryPop(0, destination, header) == PollEventResult::kEvent;
  }
  g_count_allocations.store(false, std::memory_order_release);
  _CrtSetAllocHook(previous_hook);

  EXPECT_TRUE(operations_succeeded);
  EXPECT_EQ(g_allocation_count.load(std::memory_order_relaxed), 0U);
  sender.Dispose();
#else
  GTEST_SKIP() << "MSVC Debug CRT allocation hook is required";
#endif
}

}  // namespace
}  // namespace rionetlite::detail
