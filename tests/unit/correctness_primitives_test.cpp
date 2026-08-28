#include "../../RIONetLite/impl/ring_buffer.h"
#include "../../RIONetLite/impl/server_net_init.h"
#include "../../RIONetLite/impl/session_manager.h"
#include "detail/session_lifecycle.h"
#include "detail/shutdown_coordinator.h"
#include "fake_rio_api.h"
#include "fake_socket_api.h"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <limits>
#include <random>
#include <thread>
#include <vector>

namespace rionetlite::detail {
namespace {

NetConfig FaultInjectionConfig() {
  NetConfig config;
  config.port_number_ = 32498;
  config.io_work_thread_count_ = 1;
  config.max_session_count_ = 2;
  config.session_cfg_.max_try_1recv_buffer_size = 64;
  config.session_cfg_.max_recv_buffer_size_ = 4096;
  config.session_cfg_.max_send_packet_size_ = 256;
  config.session_cfg_.max_send_count_per_1sec_ = 2;
  config.session_cfg_.max_send_buffer_size_ = 4096;
  return config;
}

NetErrorCode InitializeWithInjectedSockets(test::FakeSocketApi& socket_api,
                                           test::FakeRioApi& rio_api) {
  WinsockRuntime winsock;
  UniqueSocket listen_socket;
  return ServerNetManagerInit::Initialize(winsock, listen_socket, rio_api,
                                          socket_api, FaultInjectionConfig());
}

TEST(SessionStateTest, EnforcesLifecycleAndChangesGenerationOnReuse) {
  SessionLifecycle session(7);
  EXPECT_TRUE(session.TryReserve());
  const SessionId first = session.id();
  EXPECT_EQ(first.index, 7U);
  EXPECT_NE(first.generation, 0U);
  EXPECT_TRUE(session.TryActivate());
  EXPECT_FALSE(session.TryActivate());
  EXPECT_TRUE(session.TryBeginClose());
  EXPECT_TRUE(session.TryMarkClosing());
  EXPECT_TRUE(session.TryNotifyClose());
  EXPECT_FALSE(session.TryNotifyClose());
  EXPECT_TRUE(session.TryQuarantine());
  EXPECT_FALSE(session.TryRecycle(1, 0));
  EXPECT_TRUE(session.TryRecycle(0, 0));

  EXPECT_TRUE(session.TryReserve());
  EXPECT_NE(session.id().generation, first.generation);
  EXPECT_FALSE(session.Matches(first));
}

TEST(SessionStateTest, OnlyOneConcurrentCloseRequestWins) {
  SessionLifecycle session(1);
  ASSERT_TRUE(session.TryReserve());
  ASSERT_TRUE(session.TryActivate());
  std::atomic<int> winners = 0;
  std::vector<std::thread> threads;
  for (int index = 0; index < 16; ++index) {
    threads.emplace_back([&]() {
      if (session.TryBeginClose()) {
        winners.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  EXPECT_EQ(winners.load(std::memory_order_relaxed), 1);
}

TEST(SessionStateTest, ReusesOneSlotOneHundredThousandTimesWithoutStaleMatch) {
  SessionLifecycle session(9);
  SessionId previous{};
  for (std::uint32_t iteration = 0; iteration < 100'000; ++iteration) {
    ASSERT_TRUE(session.TryReserve()) << "iteration=" << iteration;
    EXPECT_NE(session.id().generation, 0U);
    EXPECT_FALSE(session.Matches(previous));
    previous = session.id();
    ASSERT_TRUE(session.TryActivate());
    ASSERT_TRUE(session.TryBeginClose());
    ASSERT_TRUE(session.TryMarkClosing());
    ASSERT_TRUE(session.TryNotifyClose());
    ASSERT_TRUE(session.TryQuarantine());
    ASSERT_TRUE(session.TryRecycle(0, 0));
  }
}

TEST(SessionStateTest, GenerationOverflowSkipsZero) {
  SessionLifecycle session(2, std::numeric_limits<std::uint64_t>::max());
  ASSERT_TRUE(session.TryReserve());
  EXPECT_EQ(session.id().generation, 1U);
}

TEST(RecvBufferTest, DistinguishesFullFromEmptyAndRejectsOverCommit) {
  SYSTEM_INFO system_info{};
  GetSystemInfo(&system_info);
  const auto capacity = static_cast<std::uint32_t>(system_info.dwPageSize);
  RingBufferVA2 buffer;
  ASSERT_EQ(std::get<0>(buffer.Create(capacity)), 0);
  EXPECT_EQ(buffer.GetWritableSize(), capacity);
  ASSERT_TRUE(buffer.Write(capacity));
  EXPECT_EQ(buffer.GetWritableSize(), 0U);
  EXPECT_FALSE(buffer.Write(1));
  const auto [readable, data] = buffer.Read();
  EXPECT_EQ(readable, capacity);
  EXPECT_NE(data, nullptr);
  buffer.IncremantReadPos(capacity + 1);
  EXPECT_EQ(std::get<0>(buffer.Read()), capacity);
  buffer.IncremantReadPos(capacity);
  EXPECT_EQ(std::get<0>(buffer.Read()), 0U);
  EXPECT_EQ(buffer.GetWritableSize(), capacity);
}

TEST(RecvBufferTest, ExposesContiguousSpansAcrossWrapBoundary) {
  SYSTEM_INFO system_info{};
  GetSystemInfo(&system_info);
  const auto capacity = static_cast<std::uint32_t>(system_info.dwPageSize);
  RingBufferVA2 buffer;
  ASSERT_EQ(std::get<0>(buffer.Create(capacity)), 0);
  ASSERT_TRUE(buffer.Write(capacity - 4));
  buffer.IncremantReadPos(capacity - 8);
  const std::array payload = {'r', 'i', 'o', 'n', 'e', 't', '!', '!'};
  char* written =
      buffer.Write(static_cast<std::uint32_t>(payload.size()), payload.data());
  ASSERT_NE(written, nullptr);
  EXPECT_TRUE(std::equal(payload.begin(), payload.end(), written));
  const auto [readable, data] = buffer.Read();
  EXPECT_EQ(readable, 12U);
  EXPECT_NE(data, nullptr);
}

TEST(IoContextTest, TracksPostRollbackPartialAndFinalCompletion) {
  RioIoContext context;
  context.Init(3, IOType::kSEND, 7);
  const auto buffer_id = reinterpret_cast<RIO_BUFFERID>(1);
  EXPECT_FALSE(context.BeginRequest(9, buffer_id, 8, 9, 16));
  ASSERT_TRUE(context.BeginRequest(9, buffer_id, 8, 8, 16));
  EXPECT_FALSE(context.BeginRequest(9, buffer_id, 0, 1, 16));
  EXPECT_EQ(context.request_number_, 1U);
  EXPECT_EQ(context.owner_worker_, 7U);
  EXPECT_EQ(context.session_index_, 3);
  EXPECT_EQ(context.session_unique_Id_, 9U);
  EXPECT_EQ(context.posted_length_, 8U);
  ASSERT_TRUE(context.CompleteSend(3));
  EXPECT_EQ(context.Offset, 11U);
  EXPECT_EQ(context.Length, 5U);
  EXPECT_EQ(context.completed_length_, 3U);
  EXPECT_TRUE(context.CompleteSend(5));
  EXPECT_TRUE(context.IsIdle());
  EXPECT_FALSE(context.CompleteSend(1));

  ASSERT_TRUE(context.BeginRequest(10, buffer_id, 0, 4, 16));
  EXPECT_EQ(context.request_number_, 2U);
  EXPECT_FALSE(context.CompleteSend(5));
  context.Fail();
  EXPECT_TRUE(context.IsIdle());

  // 링 버퍼가 진행 상태의 원본이므로 부분 완료 후 컨텍스트를 idle로 되돌려
  // 남은 바이트와 새로 쌓인 바이트를 하나의 요청으로 다시 게시한다.
  ASSERT_TRUE(context.BeginRequest(11, buffer_id, 0, 8, 16));
  ASSERT_TRUE(context.CompleteSend(3));
  EXPECT_FALSE(context.IsIdle());
  context.Retire();
  EXPECT_TRUE(context.IsIdle());
  EXPECT_TRUE(context.BeginRequest(11, buffer_id, 3, 5, 16));
}

TEST(ShutdownCoordinatorTest, ElectsOneOwnerAndPreservesFirstStopMode) {
  ShutdownCoordinator first_mode;
  ASSERT_TRUE(first_mode.Begin(StopMode::kGraceful));
  EXPECT_FALSE(first_mode.Begin(StopMode::kImmediate));
  EXPECT_FALSE(first_mode.immediate());
  first_mode.Complete();

  ShutdownCoordinator coordinator;
  std::atomic<int> owners = 0;
  std::vector<std::thread> threads;
  for (int index = 0; index < 8; ++index) {
    threads.emplace_back([&, index] {
      if (coordinator.Begin(StopMode::kImmediate)) {
        owners.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }
  for (auto& thread : threads) thread.join();
  EXPECT_EQ(owners.load(std::memory_order_relaxed), 1);
  EXPECT_TRUE(coordinator.stopping());
  EXPECT_TRUE(coordinator.immediate());
  coordinator.Complete();
  coordinator.Wait();
  EXPECT_TRUE(coordinator.completed());
}

TEST(RioApiTest, ProducesDeterministicErrorAndCorruptCompletions) {
  test::FakeRioApi rio;
  RIORESULT completion{};
  completion.Status = WSAECONNRESET;
  completion.BytesTransferred = 17;
  rio.completions.push_back(completion);
  RIORESULT actual{};
  ASSERT_EQ(rio.DequeueCompletion(rio.completion_queue, &actual, 1), 1U);
  EXPECT_EQ(actual.Status, WSAECONNRESET);
  EXPECT_EQ(actual.BytesTransferred, 17U);

  rio.corrupt_completion_queue = true;
  EXPECT_EQ(rio.DequeueCompletion(rio.completion_queue, &actual, 1),
            RIO_CORRUPT_CQ);
}

TEST(RioApiTest, FailsConfiguredNthCallForEveryFallibleRioOperation) {
  test::FakeRioApi rio;
  rio.initialize_fail_on_call = 2;
  rio.create_completion_queue_fail_on_call = 2;
  rio.create_request_queue_fail_on_call = 2;
  rio.receive_fail_on_call = 2;
  rio.send_fail_on_call = 2;
  rio.register_fail_on_call = 2;

  RIO_BUF buffer{};
  EXPECT_TRUE(rio.Initialize(INVALID_SOCKET));
  EXPECT_FALSE(rio.Initialize(INVALID_SOCKET));
  EXPECT_NE(rio.CreateCompletionQueue(1, nullptr), RIO_INVALID_CQ);
  EXPECT_EQ(rio.CreateCompletionQueue(1, nullptr), RIO_INVALID_CQ);
  EXPECT_NE(
      rio.CreateRequestQueue(INVALID_SOCKET, 1, 1, 1, 1, rio.completion_queue,
                             rio.completion_queue, nullptr),
      RIO_INVALID_RQ);
  EXPECT_EQ(
      rio.CreateRequestQueue(INVALID_SOCKET, 1, 1, 1, 1, rio.completion_queue,
                             rio.completion_queue, nullptr),
      RIO_INVALID_RQ);
  EXPECT_TRUE(rio.Receive(rio.request_queue, &buffer, 1, 0, nullptr));
  EXPECT_FALSE(rio.Receive(rio.request_queue, &buffer, 1, 0, nullptr));
  EXPECT_TRUE(rio.Send(rio.request_queue, &buffer, 1, 0, nullptr));
  EXPECT_FALSE(rio.Send(rio.request_queue, &buffer, 1, 0, nullptr));
  EXPECT_NE(rio.RegisterBuffer(nullptr, 1), RIO_INVALID_BUFFERID);
  EXPECT_EQ(rio.RegisterBuffer(nullptr, 1), RIO_INVALID_BUFFERID);
}

TEST(SocketApiTest, FailsConfiguredNthInitializationCallAndRollsBackSocket) {
  const auto verify_failure = [](auto configure,
                                 const NetErrorCode expected_error) {
    test::FakeSocketApi socket_api;
    test::FakeRioApi rio_api;
    configure(socket_api);
    EXPECT_EQ(InitializeWithInjectedSockets(socket_api, rio_api),
              NetErrorCode::kNONE);
    EXPECT_EQ(InitializeWithInjectedSockets(socket_api, rio_api),
              expected_error);
  };

  verify_failure([](auto& api) { api.create_fail_on_call = 2; },
                 NetErrorCode::kLISTEN_SOCKET_CREATE_FAIL);
  verify_failure([](auto& api) { api.set_option_fail_on_call = 2; },
                 NetErrorCode::kLISTEN_SOCKET_BIND_FAIL);
  verify_failure([](auto& api) { api.bind_fail_on_call = 2; },
                 NetErrorCode::kLISTEN_SOCKET_BIND_FAIL);
  verify_failure([](auto& api) { api.listen_fail_on_call = 2; },
                 NetErrorCode::kLISTEN_SOCKET_LISTEN_FAIL);
}

NetConfig MultiWorkerConfig() {
  NetConfig config = FaultInjectionConfig();
  config.io_work_thread_count_ = 3;
  config.max_session_count_ = 7;
  return config;
}

TEST(SessionPoolTest, LeasesSessionsRoundRobinAcrossWorkersFromColdStart) {
  test::FakeRioApi rio;
  SessionManager sessions;
  ASSERT_TRUE(sessions.PrepareSessionPool(MultiWorkerConfig(), rio));

  // 기동 직후 첫 접속들이 worker 0에 몰리지 않아야 한다.
  const std::array expected_workers = {0, 1, 2, 0, 1, 2, 0};
  for (std::size_t index = 0; index < expected_workers.size(); ++index) {
    auto* session = sessions.AllocClientSession();
    ASSERT_NE(session, nullptr) << "index=" << index;
    EXPECT_EQ(session->GetRioThreadId(), expected_workers[index])
        << "index=" << index;
  }
  EXPECT_EQ(sessions.AllocClientSession(), nullptr);
  sessions.Dispose();
}

TEST(SessionPoolTest, ForcedCloseIgnoresAStaleGeneration) {
  test::FakeRioApi rio;
  SessionManager sessions;
  ASSERT_TRUE(sessions.PrepareSessionPool(MultiWorkerConfig(), rio));
  auto* session = sessions.AllocClientSession();
  ASSERT_NE(session, nullptr);
  const auto index = static_cast<std::int32_t>(session->GetIndex());
  const auto generation = session->GetUID();

  // 송신 실패와 종료 사이에 슬롯이 재사용되어도 새 세션을 끊으면 안 된다.
  EXPECT_EQ(sessions.ForcedClose(index, generation + 1), -1);
  EXPECT_GE(sessions.ForcedClose(index, generation), 0);
  sessions.Dispose();
}

TEST(RioApiTest, SessionPoolRollsBackSuccessfulBuffersOnNthRegisterFailure) {
  test::FakeRioApi rio;
  rio.register_fail_on_call = 3;
  SessionManager sessions;

  EXPECT_FALSE(sessions.PrepareSessionPool(FaultInjectionConfig(), rio));
  sessions.Dispose();
  EXPECT_EQ(rio.register_buffer_calls, 3U);
  EXPECT_EQ(rio.deregister_buffer_calls, 2U);
}

}  // namespace
}  // namespace rionetlite::detail
