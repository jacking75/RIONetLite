#pragma once

#include "../define.h"
#include "session_manager.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace rionetlite {
// One preallocated mailbox per session; duplicate close requests coalesce.
// Only the owning I/O worker invokes disconnect callbacks.
class ServerNetLazyedDisConnManager {
 public:
  void PrepareSessionQueues(const std::uint32_t thread_count,
                            const std::uint32_t drain_timeout_ms,
                            const SessionManager& session_manager) {
    drain_timeout_ms_ = drain_timeout_ms;
    queues_.clear();
    queues_.reserve(thread_count);
    for (std::uint32_t worker = 0; worker < thread_count; ++worker) {
      const auto partition = session_manager.WorkerPartition(worker);
      auto queue = std::make_unique<Queue>();
      queue->first_index = partition.first_index;
      queue->entries.resize(partition.count);
      queue->pending.resize(partition.count, false);
      queue->ready.resize(partition.count);
      queues_.push_back(std::move(queue));
    }
  }

  void AddSession(const int worker, const std::int32_t index,
                  const std::uint64_t uid, const DisconnectReason reason) {
    Add(worker, {index, uid, reason, 0});
  }

  void ProcessLazyedDisConnectSession(const int worker,
                                      SessionManager* session_manager) {
    // Deferral cannot turn this into an unbounded drain loop or allocate
    // memory.
    const auto budget = std::min<std::size_t>(Pending(worker), 64);
    const auto now = GetTickCount64();
    for (std::size_t processed = 0; processed < budget; ++processed) {
      auto item = Fetch(worker);
      if (item.uid == 0) break;
      auto* session =
          session_manager->GetSessionObj(item.session_index_, item.uid);
      if (session == nullptr) continue;
      if (item.drain_deadline_ms_ != 0 && now >= item.drain_deadline_ms_) {
        item.reason_ = DisconnectReason::kSEND_ERROR;
        item.drain_deadline_ms_ = 0;
      }
      if (!DisConnectFunc(session, item.uid, item.reason_)) {
        if (item.drain_deadline_ms_ == 0) {
          item.drain_deadline_ms_ = now + drain_timeout_ms_;
        }
        Add(worker, item);
      }
    }
  }

  std::function<bool(TcpSession*, std::uint64_t, DisconnectReason)>
      DisConnectFunc;

 private:
  struct Queue {
    std::mutex mutex;
    std::uint32_t first_index = 0;
    std::vector<LazyedDisConnectSession> entries;
    std::vector<bool> pending;
    std::vector<std::uint32_t> ready;
    std::size_t head = 0;
    std::size_t count = 0;
  };

  void Add(const int worker, const LazyedDisConnectSession item) {
    auto& queue = *queues_[worker];
    const auto local =
        static_cast<std::uint32_t>(item.session_index_) - queue.first_index;
    if (local >= queue.entries.size() || item.uid == 0) return;
    std::scoped_lock lock(queue.mutex);
    auto& entry = queue.entries[local];
    if (queue.pending[local]) {
      // Explicit/error close takes priority over deferred peer FIN. A delayed
      // old generation cannot replace a newer command.
      if (entry.uid > item.uid) return;
      if (entry.uid == item.uid &&
          entry.reason_ != DisconnectReason::kZERO_COMPLETION)
        return;
      entry = item;
      return;
    }
    entry = item;
    queue.pending[local] = true;
    queue.ready[(queue.head + queue.count) % queue.ready.size()] = local;
    ++queue.count;
  }

  std::size_t Pending(const int worker) {
    auto& queue = *queues_[worker];
    std::scoped_lock lock(queue.mutex);
    return queue.count;
  }

  LazyedDisConnectSession Fetch(const int worker) {
    auto& queue = *queues_[worker];
    std::scoped_lock lock(queue.mutex);
    if (queue.count == 0) return {};
    const auto local = queue.ready[queue.head];
    queue.head = (queue.head + 1) % queue.ready.size();
    --queue.count;
    queue.pending[local] = false;
    return queue.entries[local];
  }

  std::uint32_t drain_timeout_ms_ = 5000;
  std::vector<std::unique_ptr<Queue>> queues_;
};
}  // namespace rionetlite
