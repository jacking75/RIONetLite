#pragma once

#include "../../src/detail/processor_topology.h"
#include "../../src/detail/rio_api.h"
#include "../../src/detail/win32_resources.h"
#include "../define.h"
#include "../i_net_event.h"
#include "server_net_lazyed_dis_conn_manager.h"
#include "session_manager.h"
#include "tcp_session.h"

#include <intrin.h>
#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <thread>
#include <vector>

namespace rionetlite {
class ServerNetIOWorker {
 public:
  ServerNetIOWorker() = default;
  ~ServerNetIOWorker() = default;

  std::function<bool(TcpSession*, const uint64_t, const DisconnectReason)>
      DisConnectFunc;
  void Init(ServerNetLazyedDisConnManager* p_lazyed_dis_conn_mgr,
            SessionManager* p_session_mgr, INetEventManager* p_net_event_mgr,
            detail::RioApi& rio_api) {
    pref_lazyed_dis_conn_mgr_ = p_lazyed_dis_conn_mgr;
    pref_session_mgr_ = p_session_mgr;
    pref_net_event_mgr_ = p_net_event_mgr;
    rio_api_ = &rio_api;
  }

  bool StartIoThreads(
      const NetConfig& config,
      std::vector<detail::UniqueRioCompletionQueue>& rio_completion_queues) {
    try {
      is_run_.store(true, std::memory_order_release);
      timed_out_.store(false, std::memory_order_release);
      corrupt_completion_queue_.store(false, std::memory_order_release);
      completion_batch_size_ = config.completion_batch_size_;
      adaptive_spin_count_ = config.adaptive_spin_count_;
      worker_wait_mode_ = config.worker_wait_mode_;
      enable_thread_affinity_ = config.enable_thread_affinity_;
      idle_timeout_ms_ = config.idle_timeout_ms_;
      PrepareAffinityTargets();
      pref_lazyed_dis_conn_mgr_->PrepareSessionQueues(
          config.io_work_thread_count_, config.peer_close_drain_timeout_ms_,
          *pref_session_mgr_);

      worker_results_.assign(config.io_work_thread_count_,
                             std::vector<RIORESULT>(completion_batch_size_));
      io_work_threads_.reserve(config.io_work_thread_count_);

      rio_completion_queues.reserve(config.io_work_thread_count_);
      notification_events_.reserve(config.io_work_thread_count_);
      for (std::int32_t i = 0; i < config.io_work_thread_count_; ++i) {
        RIO_NOTIFICATION_COMPLETION notification{};
        PRIO_NOTIFICATION_COMPLETION notification_pointer = nullptr;
        if (worker_wait_mode_ != 0) {
          detail::UniqueHandle event(
              CreateEvent(nullptr, TRUE, FALSE, nullptr));
          if (!event) {
            is_run_.store(false, std::memory_order_release);
            rio_completion_queues.clear();
            notification_events_.clear();
            return false;
          }
          notification.Type = RIO_EVENT_COMPLETION;
          notification.Event.EventHandle = event.get();
          notification.Event.NotifyReset = TRUE;
          notification_pointer = &notification;
          notification_events_.push_back(std::move(event));
        } else {
          notification_events_.emplace_back();
        }
        auto rio_queue = rio_api_->CreateCompletionQueue(
            config.completion_queue_capacity_, notification_pointer);
        if (rio_queue == RIO_INVALID_CQ) {
          is_run_.store(false, std::memory_order_release);
          rio_completion_queues.clear();
          notification_events_.clear();
          return false;
        }
        rio_completion_queues.emplace_back(*rio_api_, rio_queue);
      }

      for (std::int32_t i = 0; i < config.io_work_thread_count_; ++i) {
        const auto rio_queue = rio_completion_queues[i].get();
        const auto notification_event = notification_events_[i].get();
        io_work_threads_.emplace_back(
            [i, rio_queue, notification_event, this]() {
              IoWorkerThread(i, rio_queue, notification_event);
            });
      }
    } catch (...) {
      is_run_.store(false, std::memory_order_release);
      WakeWorkers();
      for (auto& thread : io_work_threads_) {
        if (thread.joinable()) {
          thread.join();
        }
      }
      io_work_threads_.clear();
      rio_completion_queues.clear();
      return false;
    }

    return true;
  }

  bool Stop(const std::uint32_t timeout_ms) {
    is_run_.store(false, std::memory_order_release);
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms);
    shutdown_deadline_ns_.store(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            deadline.time_since_epoch())
            .count(),
        std::memory_order_release);
    WakeWorkers();

    for (auto& thread : io_work_threads_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
    io_work_threads_.clear();

    return !timed_out_.load(std::memory_order_acquire) &&
           !corrupt_completion_queue_.load(std::memory_order_acquire);
  }

  [[nodiscard]] bool corrupt_completion_queue() const noexcept {
    return corrupt_completion_queue_.load(std::memory_order_acquire);
  }

  // CQ must be closed before its notification handle is released.
  void ReleaseNotificationEvents() noexcept {
    notification_events_.clear();
    worker_results_.clear();
  }

  // A deliberately retained CQ can still refer to its notification handle.
  void AbandonNotificationEvents() noexcept {
    for (auto& event : notification_events_) {
      static_cast<void>(event.Release());
    }
  }

 private:
  void IoWorkerThread(const int thread_index, RIO_CQ rio_completion_queues,
                      HANDLE notification_event) {
    ApplyThreadAffinity(static_cast<std::uint32_t>(thread_index));
    auto& results = worker_results_[thread_index];
    std::uint32_t empty_polls = 0;
    bool notification_armed = false;
    std::uint64_t next_idle_sweep_ms = GetTickCount64() + kIdleSweepIntervalMs;

    while (is_run_.load(std::memory_order_acquire) ||
           pref_session_mgr_->AnyOutstandingOrActive()) {
      if (!is_run_.load(std::memory_order_acquire) && ShutdownTimedOut()) {
        timed_out_.store(true, std::memory_order_release);
        break;
      }
      ULONG numResults = rio_api_->DequeueCompletion(
          rio_completion_queues, results.data(), completion_batch_size_);

      if (RIO_CORRUPT_CQ == numResults) {
        corrupt_completion_queue_.store(true, std::memory_order_release);
        is_run_.store(false, std::memory_order_release);
        pref_net_event_mgr_->OnCorruptCompletionQueue(thread_index);
        break;
      }

      for (ULONG i = 0; i < numResults; ++i) {
        auto context =
            reinterpret_cast<RioIoContext*>(results[i].RequestContext);
        auto pSession =
            context == nullptr
                ? nullptr
                : pref_session_mgr_->GetSessionObj(context->session_index_);
        auto transferred = results[i].BytesTransferred;

        ProcessRIOCQ(thread_index, results[i].Status, transferred, context,
                     pSession);
      }

      pref_lazyed_dis_conn_mgr_->ProcessLazyedDisConnectSession(
          thread_index, pref_session_mgr_);

      if (idle_timeout_ms_ != 0) {
        if (const auto now = GetTickCount64(); now >= next_idle_sweep_ms) {
          SweepIdleSessions(thread_index, now);
          next_idle_sweep_ms = now + kIdleSweepIntervalMs;
        }
      }

      if (0 == numResults) {
        WaitForWork(empty_polls, notification_armed, rio_completion_queues,
                    notification_event, thread_index);
      } else {
        empty_polls = 0;
      }
    }
  }

  void ProcessRIOCQ(const int worker, const LONG status,
                    const ULONG transferred, RioIoContext* p_context,
                    TcpSession* p_session) {
    if (p_context == nullptr) {
      pref_net_event_mgr_->OnStaleCompletion(worker, -1, 0);
      return;
    }
    if (p_context->owner_worker_ != static_cast<std::uint32_t>(worker)) {
      pref_net_event_mgr_->OnTransportError(
          worker, p_context->session_index_, p_context->session_unique_Id_,
          "completion_owner_mismatch", p_context->owner_worker_);
      if (p_session != nullptr &&
          p_session->GetUID() == p_context->session_unique_Id_) {
        if (p_context->io_type_ == IOType::kRECV) {
          p_session->RecvFailed();
        } else if (p_context->io_type_ == IOType::kSEND) {
          p_session->SendFailed(p_context);
        }
        static_cast<void>(
            DisConnectFunc(p_session, p_context->session_unique_Id_,
                           p_context->io_type_ == IOType::kSEND
                               ? DisconnectReason::kSEND_ERROR
                               : DisconnectReason::kIO_REQUEST_RECV_ERROR));
      }
      return;
    }
    auto cq_session_uid = p_context->session_unique_Id_;

    if (p_session == nullptr || p_session->GetUID() != cq_session_uid) {
      pref_net_event_mgr_->OnStaleCompletion(worker, p_context->session_index_,
                                             cq_session_uid);
      return;
    }

    if (status != NO_ERROR) {
      pref_net_event_mgr_->OnTransportError(
          worker, p_context->session_index_, cq_session_uid,
          p_context->io_type_ == IOType::kRECV ? "receive" : "send",
          static_cast<std::uint32_t>(status));
      if (p_context->io_type_ == IOType::kRECV) {
        p_session->RecvFailed();
      } else if (p_context->io_type_ == IOType::kSEND) {
        p_session->SendFailed(p_context);
      }
      DisConnectFunc(p_session, cq_session_uid,
                     p_context->io_type_ == IOType::kRECV
                         ? DisconnectReason::kIO_REQUEST_RECV_ERROR
                         : DisconnectReason::kSEND_ERROR);
      return;
    }

    if (transferred == 0) {
      if (p_context->io_type_ == IOType::kRECV) {
        p_session->RecvFailed();
        p_session->BeginPeerClose();
        pref_lazyed_dis_conn_mgr_->AddSession(
            p_session->GetRioThreadId(), p_session->GetIndex(), cq_session_uid,
            DisconnectReason::kZERO_COMPLETION);
      } else if (p_context->io_type_ == IOType::kSEND) {
        p_session->SendFailed(p_context);
        static_cast<void>(DisConnectFunc(p_session, cq_session_uid,
                                         DisconnectReason::kZERO_COMPLETION));
      }

    } else if (IOType::kRECV == p_context->io_type_) {
      // 종료가 먼저 관찰된 세션의 지연 완료다. OnClose 뒤에 OnData를 전달하지
      // 않도록 콜백 없이 요청만 정리한다.
      if (!p_session->IsConnected()) {
        p_session->RecvFailed();
        return;
      }

      if (!p_session->RecvCompletion(transferred)) {
        pref_net_event_mgr_->OnTransportError(
            worker, p_session->GetIndex(), cq_session_uid,
            "receive_completion_length", transferred);
        DisConnectFunc(p_session, cq_session_uid,
                       DisconnectReason::kPROTOCOL_ERROR);
        return;
      }

      // 콜백이 도는 동안에도 커널이 수신할 수 있도록 여유가 있으면 다음 수신을
      // 먼저 게시한다. 커널이 쓰는 구간과 콜백이 읽는 구간은 링에서 서로
      // 겹치지 않는다.
      bool posted = false;
      if (const auto result = p_session->PostRecv();
          result == NetErrorCode::kNONE) {
        posted = true;
      } else if (result != NetErrorCode::kRECV_BUFFER_FULL) {
        DisConnectFunc(p_session, cq_session_uid,
                       DisconnectReason::kIO_REQUEST_RECV_ERROR);
        return;
      }

      if (!InvokeReceiveCallback(worker, p_session)) {
        DisConnectFunc(p_session, cq_session_uid,
                       DisconnectReason::kPROTOCOL_ERROR);
        return;
      }

      if (!posted) {
        PostRecv(cq_session_uid, p_session);
      }

    } else if (IOType::kSEND == p_context->io_type_) {
      if (transferred <= p_context->Length) {
        pref_net_event_mgr_->OnSendCompleted(p_session->GetIndex(),
                                             transferred);
      }
      if (!p_session->SendCompletion(p_context, transferred)) {
        DisConnectFunc(p_session, cq_session_uid,
                       DisconnectReason::kPARITAL_SEND_COMPLETION);
      }
    } else {
      pref_net_event_mgr_->OnTransportError(worker, p_context->session_index_,
                                            cq_session_uid, "unknown", 0);
    }
  }

  // 받기 버퍼의 읽은 위치도 여기서 이동 시키므로 링버퍼의 데이터가 겹치는 것을
  // 막고 싶으면 애플리케이션에서 OnReceive의 데이터를 다 사용할 때까지 잘
  // 보관해야 한다.
  bool InvokeReceiveCallback(const int worker, TcpSession* p_session) {
    auto [readSize, pReadData] = p_session->ReadRecvBuffer();
    auto usedSize = pref_net_event_mgr_->OnReceive(
        p_session->GetIndex(), p_session->GetUID(), readSize, pReadData);
    if (!p_session->ConsumeRecvBuffer(usedSize)) {
      pref_net_event_mgr_->OnTransportError(
          worker, p_session->GetIndex(), p_session->GetUID(),
          "receive_callback_consumed_too_much", 0);
      return false;
    }
    return true;
  }

  bool PostRecv(uint64_t cq_session_uid, TcpSession* p_session) {
    if (auto result = p_session->PostRecv(); result != NetErrorCode::kNONE) {
      if (result == NetErrorCode::kRECV_RIO_FAIL) {
        DisConnectFunc(p_session, cq_session_uid,
                       DisconnectReason::kIO_REQUEST_RECV_ERROR);
      } else if (result == NetErrorCode::kRECV_BUFFER_FULL) {
        DisConnectFunc(p_session, cq_session_uid,
                       DisconnectReason::kRECV_BUFFER_FULL);
      }
      return false;
    }

    return true;
  }

  // 자기 파티션의 세션만 약 1초 주기로 훑는다. 무통신 상태로 사라진 피어가
  // 세션 슬롯과 등록 버퍼를 영구히 점유하는 것을 막는다.
  void SweepIdleSessions(const int thread_index, const std::uint64_t now_ms) {
    const auto partition = pref_session_mgr_->WorkerPartition(
        static_cast<std::uint32_t>(thread_index));
    for (std::uint32_t offset = 0; offset < partition.count; ++offset) {
      const std::uint32_t index = partition.first_index + offset;
      auto* session = pref_session_mgr_->GetSessionObj(index);
      if (session == nullptr || !session->IsConnected() ||
          !session->IsIdleFor(now_ms, idle_timeout_ms_)) {
        continue;
      }
      const auto uid = session->GetUID();
      if (pref_session_mgr_->ForcedClose(static_cast<std::int32_t>(index),
                                         uid) >= 0) {
        pref_lazyed_dis_conn_mgr_->AddSession(
            thread_index, static_cast<std::int32_t>(index), uid,
            DisconnectReason::kIDLE_TIMEOUT);
      }
    }
  }

  void WaitForWork(std::uint32_t& empty_polls, bool& notification_armed,
                   const RIO_CQ queue, const HANDLE notification_event,
                   const int worker) {
    if (worker_wait_mode_ == 0 ||
        (worker_wait_mode_ == 1 && empty_polls++ < adaptive_spin_count_)) {
      YieldProcessor();
      return;
    }
    if (notification_event == nullptr) {
      std::this_thread::yield();
      return;
    }
    if (!notification_armed) {
      const int notify_result = rio_api_->Notify(queue);
      if (notify_result == ERROR_SUCCESS || notify_result == WSAEALREADY) {
        notification_armed = true;
      } else {
        pref_net_event_mgr_->OnTransportError(
            worker, -1, 0, "notify", static_cast<std::uint32_t>(notify_result));
        std::this_thread::yield();
        return;
      }
    }
    // A bounded wait also services non-RIO control work such as close queues.
    if (WaitForSingleObject(notification_event, 50) == WAIT_OBJECT_0) {
      notification_armed = false;
    }
    empty_polls = 0;
  }

  void WakeWorkers() noexcept {
    for (const auto& event : notification_events_) {
      if (event) {
        static_cast<void>(SetEvent(event.get()));
      }
    }
  }

  void PrepareAffinityTargets() {
    affinity_targets_.clear();
    if (!enable_thread_affinity_) return;

    std::vector<detail::ProcessorLocation> processors;
    bool numa_topology_available = true;
    const WORD group_count = GetActiveProcessorGroupCount();
    for (WORD group = 0; group < group_count; ++group) {
      const DWORD processor_count = GetActiveProcessorCount(group);
      for (DWORD number = 0; number < processor_count; ++number) {
        PROCESSOR_NUMBER processor_number{};
        processor_number.Group = group;
        processor_number.Number = static_cast<BYTE>(number);
        USHORT numa_node = 0;
        if (GetNumaProcessorNodeEx(&processor_number, &numa_node) == FALSE ||
            numa_node == std::numeric_limits<USHORT>::max()) {
          numa_topology_available = false;
          numa_node = 0;
        }
        processors.push_back(detail::ProcessorLocation{
            .group = group,
            .number = static_cast<std::uint8_t>(number),
            .numa_node = numa_node});
      }
    }

    const auto ordered = numa_topology_available
                             ? detail::OrderProcessorsForAffinity(processors)
                             : processors;
    affinity_targets_.reserve(ordered.size());
    for (const auto& processor : ordered) {
      PROCESSOR_NUMBER target{};
      target.Group = processor.group;
      target.Number = processor.number;
      affinity_targets_.push_back(target);
    }
  }

  void ApplyThreadAffinity(const std::uint32_t thread_index) {
    if (!enable_thread_affinity_ || thread_index >= affinity_targets_.size()) {
      return;
    }
    const auto& target = affinity_targets_[thread_index];
    GROUP_AFFINITY affinity{};
    affinity.Group = target.Group;
    affinity.Mask = static_cast<KAFFINITY>(1) << target.Number;
    static_cast<void>(
        SetThreadGroupAffinity(GetCurrentThread(), &affinity, nullptr));
  }

  bool ShutdownTimedOut() const noexcept {
    const auto deadline = shutdown_deadline_ns_.load(std::memory_order_acquire);
    if (deadline == 0) {
      return false;
    }
    const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();
    return now >= deadline;
  }

  static constexpr std::uint64_t kIdleSweepIntervalMs = 1000;

  std::atomic<bool> is_run_{false};
  std::atomic<bool> timed_out_{false};
  std::atomic<bool> corrupt_completion_queue_{false};
  std::atomic<std::int64_t> shutdown_deadline_ns_{0};
  std::uint32_t completion_batch_size_ = 64;
  std::uint32_t adaptive_spin_count_ = 128;
  std::uint32_t idle_timeout_ms_ = 0;
  std::uint8_t worker_wait_mode_ = 1;
  bool enable_thread_affinity_ = false;

  ServerNetLazyedDisConnManager* pref_lazyed_dis_conn_mgr_ = nullptr;
  SessionManager* pref_session_mgr_ = nullptr;
  INetEventManager* pref_net_event_mgr_ = nullptr;
  detail::RioApi* rio_api_ = nullptr;

  std::vector<std::thread> io_work_threads_;
  std::vector<std::vector<RIORESULT>> worker_results_;
  std::vector<detail::UniqueHandle> notification_events_;
  std::vector<PROCESSOR_NUMBER> affinity_targets_;
};

}  // namespace rionetlite
