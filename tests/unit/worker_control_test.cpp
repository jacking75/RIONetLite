#include "../../RIONetLite/impl/server_net_io_worker.h"
#include "fake_rio_api.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace rionetlite {
namespace {

NetConfig WorkerConfig(const std::int32_t workers = 1) {
  NetConfig config;
  config.io_work_thread_count_ = workers;
  config.max_session_count_ = workers;
  config.completion_batch_size_ = 8;
  config.completion_queue_capacity_ = 8;
  config.worker_wait_mode_ = 0;
  config.session_cfg_.max_recv_buffer_size_ = 4096;
  config.session_cfg_.max_send_buffer_size_ = 4096;
  return config;
}

class WorkerEvents final : public INetEventManager {
 public:
  std::atomic<std::uint32_t> stale{0};
  std::atomic<std::uint32_t> corrupt{0};
  void OnStaleCompletion(std::int32_t, std::int32_t, std::uint64_t) override {
    ++stale;
  }
  void OnCorruptCompletionQueue(std::int32_t) override { ++corrupt; }
};

TEST(WorkerControlTest, IgnoresNullContextInTheRealWorkerLoop) {
  test::FakeRioApi rio;
  rio.completions.push_back(RIORESULT{});
  SessionManager sessions;
  ASSERT_TRUE(sessions.PrepareSessionPool(WorkerConfig(), rio));
  ServerNetLazyedDisConnManager closes;
  WorkerEvents events;
  ServerNetIOWorker worker;
  worker.Init(&closes, &sessions, &events, rio);
  std::vector<detail::UniqueRioCompletionQueue> queues;
  ASSERT_TRUE(worker.StartIoThreads(WorkerConfig(), queues));
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (events.stale.load() == 0 &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_TRUE(worker.Stop(100));
  EXPECT_EQ(events.stale.load(), 1U);
  queues.clear();
  worker.ReleaseNotificationEvents();
  sessions.Dispose();
}

TEST(WorkerControlTest, CorruptQueueIsReportedAndStopFails) {
  test::FakeRioApi rio;
  rio.corrupt_completion_queue = true;
  SessionManager sessions;
  ASSERT_TRUE(sessions.PrepareSessionPool(WorkerConfig(), rio));
  ServerNetLazyedDisConnManager closes;
  WorkerEvents events;
  ServerNetIOWorker worker;
  worker.Init(&closes, &sessions, &events, rio);
  std::vector<detail::UniqueRioCompletionQueue> queues;
  ASSERT_TRUE(worker.StartIoThreads(WorkerConfig(), queues));
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (events.corrupt.load() == 0 &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_FALSE(worker.Stop(100));
  EXPECT_EQ(events.corrupt.load(), 1U);
  queues.clear();
  worker.ReleaseNotificationEvents();
  sessions.Dispose();
}

TEST(WorkerControlTest, RollsBackQueuesBeforeStartingAnyThreadOnCreateFailure) {
  test::FakeRioApi rio;
  rio.create_completion_queue_fail_on_call = 2;
  const auto config = WorkerConfig(2);
  SessionManager sessions;
  ASSERT_TRUE(sessions.PrepareSessionPool(config, rio));
  ServerNetLazyedDisConnManager closes;
  WorkerEvents events;
  ServerNetIOWorker worker;
  worker.Init(&closes, &sessions, &events, rio);
  std::vector<detail::UniqueRioCompletionQueue> queues;
  EXPECT_FALSE(worker.StartIoThreads(config, queues));
  EXPECT_TRUE(queues.empty());
  EXPECT_EQ(rio.close_completion_queue_calls, 1U);
  EXPECT_TRUE(worker.Stop(100));
  worker.ReleaseNotificationEvents();
  sessions.Dispose();
}

TEST(WorkerControlTest, CoalescesRepeatedClosesAndBoundsDeferredWork) {
  test::FakeRioApi rio;
  SessionManager sessions;
  const auto config = WorkerConfig();
  ASSERT_TRUE(sessions.PrepareSessionPool(config, rio));
  auto* session = sessions.AllocClientSession();
  ASSERT_NE(session, nullptr);
  const auto uid = session->GetUID();
  ServerNetLazyedDisConnManager closes;
  closes.PrepareSessionQueues(1, 1000, sessions);
  std::uint32_t calls = 0;
  closes.DisConnectFunc = [&](TcpSession*, std::uint64_t generation,
                              DisconnectReason reason) {
    EXPECT_EQ(generation, uid);
    EXPECT_EQ(reason, DisconnectReason::kACTIVE);
    ++calls;
    return false;
  };
  for (std::uint32_t index = 0; index < 100000; ++index) {
    closes.AddSession(0, session->GetIndex(), uid, DisconnectReason::kACTIVE);
  }
  closes.ProcessLazyedDisConnectSession(0, &sessions);
  EXPECT_EQ(calls, 1U);
  closes.ProcessLazyedDisConnectSession(0, &sessions);
  EXPECT_EQ(calls, 2U);
  session->DisConnectByAcceptFail();
  sessions.ReturnSession(session->GetIndex());
  sessions.Dispose();
}

}  // namespace
}  // namespace rionetlite
