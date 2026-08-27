#include "rionetlite/queued_event_adapter.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <limits>
#include <mutex>
#include <utility>
#include <vector>

namespace rionetlite {

namespace {

constexpr std::uint64_t SaturatingMultiply(const std::uint64_t left,
                                           const std::uint64_t right) noexcept {
  return right != 0 && left > std::numeric_limits<std::uint64_t>::max() / right
             ? std::numeric_limits<std::uint64_t>::max()
             : left * right;
}

constexpr std::uint64_t SaturatingAdd(const std::uint64_t left,
                                      const std::uint64_t right) noexcept {
  return left > std::numeric_limits<std::uint64_t>::max() - right
             ? std::numeric_limits<std::uint64_t>::max()
             : left + right;
}

}  // namespace

class QueuedEventAdapter::Impl final {
 public:
  struct Slot final {
    QueuedEventHeader header;
    std::vector<std::byte> payload;
    std::chrono::steady_clock::time_point enqueued_at;
  };

  struct Shard final {
    Shard(std::uint32_t capacity, std::uint32_t payload_bytes)
        : slots(capacity) {
      for (auto& slot : slots) {
        slot.payload.resize(payload_bytes);
      }
    }
    std::mutex mutex;
    std::vector<Slot> slots;
    std::size_t head = 0;
    std::size_t tail = 0;
    std::size_t count = 0;
  };

  struct SessionGate final {
    std::uint64_t generation = 0;
    std::uint64_t next_sequence = 0;
    bool connected = false;
    bool closed = false;
  };

  Impl(std::uint32_t shard_count, std::uint32_t events_per_shard,
       std::uint32_t max_payload_bytes, std::uint32_t max_session_count,
       EventQueueFullPolicy full_policy, CloseRequest close_request)
      : full_policy(full_policy),
        close_request(std::move(close_request)),
        max_payload_bytes(max_payload_bytes),
        session_gates(max_session_count) {
    const std::uint64_t slot_count =
        SaturatingMultiply(shard_count, events_per_shard);
    const std::uint64_t slot_bytes = SaturatingMultiply(
        slot_count,
        sizeof(Slot) + static_cast<std::uint64_t>(max_payload_bytes));
    const std::uint64_t gate_bytes =
        SaturatingMultiply(max_session_count, sizeof(SessionGate));
    const std::uint64_t fixed_bytes = SaturatingAdd(
        sizeof(Impl), SaturatingMultiply(shard_count, sizeof(Shard)));
    estimated_memory_bytes =
        SaturatingAdd(SaturatingAdd(slot_bytes, gate_bytes), fixed_bytes);
    shards.reserve(shard_count);
    for (std::uint32_t index = 0; index < shard_count; ++index) {
      shards.push_back(
          std::make_unique<Shard>(events_per_shard, max_payload_bytes));
    }
  }

  bool Push(QueuedEventHeader header, std::span<const std::byte> payload) {
    if (shards.empty() || header.session.index >= session_gates.size() ||
        payload.size() > max_payload_bytes) {
      rejected.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    Shard& shard = *shards[header.session.index % shards.size()];
    std::scoped_lock lock(shard.mutex);
    SessionGate& gate = session_gates[header.session.index];
    SessionGate next_gate = gate;
    switch (header.type) {
      case QueuedEventType::kConnected:
        if (header.session.generation == 0 ||
            header.session.generation < gate.generation ||
            (header.session.generation == gate.generation && gate.connected)) {
          rejected.fetch_add(1, std::memory_order_relaxed);
          return false;
        }
        next_gate = SessionGate{.generation = header.session.generation,
                                .next_sequence = 1,
                                .connected = true,
                                .closed = false};
        break;
      case QueuedEventType::kData:
        if (!gate.connected || gate.closed ||
            gate.generation != header.session.generation) {
          rejected.fetch_add(1, std::memory_order_relaxed);
          return false;
        }
        break;
      case QueuedEventType::kClosed:
        if (!gate.connected || gate.closed ||
            gate.generation != header.session.generation) {
          rejected.fetch_add(1, std::memory_order_relaxed);
          return false;
        }
        next_gate.closed = true;
        break;
    }
    if (shard.count == shard.slots.size()) {
      if (header.type == QueuedEventType::kClosed) {
        gate = next_gate;
      }
      rejected.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    Slot& slot = shard.slots[shard.tail];
    header.sequence = next_gate.next_sequence++;
    header.payload_bytes = payload.size();
    slot.header = header;
    slot.enqueued_at = std::chrono::steady_clock::now();
    if (!payload.empty()) {
      std::memcpy(slot.payload.data(), payload.data(), payload.size());
    }
    shard.tail = (shard.tail + 1) % shard.slots.size();
    ++shard.count;
    gate = next_gate;
    const std::uint64_t current_depth =
        depth.fetch_add(1, std::memory_order_relaxed) + 1;
    std::uint64_t current = high_watermark.load(std::memory_order_relaxed);
    while (current < current_depth &&
           !high_watermark.compare_exchange_weak(current, current_depth,
                                                 std::memory_order_relaxed)) {
    }
    return true;
  }

  EventQueueFullPolicy full_policy;
  CloseRequest close_request;
  std::uint32_t max_payload_bytes;
  std::uint64_t estimated_memory_bytes = 0;
  std::vector<std::unique_ptr<Shard>> shards;
  std::vector<SessionGate> session_gates;
  std::atomic<std::uint64_t> high_watermark{0};
  std::atomic<std::uint64_t> rejected{0};
  std::atomic<std::uint64_t> depth{0};
  std::atomic<std::uint64_t> dequeued{0};
  std::atomic<std::uint64_t> wait_total_nanoseconds{0};
  std::atomic<std::uint64_t> wait_max_nanoseconds{0};
};

namespace {

std::uint32_t DefaultMaxSessionCount(const std::uint32_t shard_count,
                                     const std::uint32_t events_per_shard) {
  const std::uint64_t product = static_cast<std::uint64_t>(shard_count) *
                                static_cast<std::uint64_t>(events_per_shard);
  return static_cast<std::uint32_t>(std::clamp<std::uint64_t>(
      product, 1, std::numeric_limits<std::uint32_t>::max()));
}

void UpdateMaximum(std::atomic<std::uint64_t>& maximum,
                   const std::uint64_t value) noexcept {
  std::uint64_t current = maximum.load(std::memory_order_relaxed);
  while (current < value && !maximum.compare_exchange_weak(
                                current, value, std::memory_order_relaxed)) {
  }
}

}  // namespace

QueuedEventAdapter::QueuedEventAdapter(const std::uint32_t shard_count,
                                       const std::uint32_t events_per_shard,
                                       const std::uint32_t max_payload_bytes,
                                       const EventQueueFullPolicy full_policy,
                                       CloseRequest close_request)
    : QueuedEventAdapter(shard_count, events_per_shard, max_payload_bytes,
                         DefaultMaxSessionCount(shard_count, events_per_shard),
                         full_policy, std::move(close_request)) {}

QueuedEventAdapter::QueuedEventAdapter(const std::uint32_t shard_count,
                                       const std::uint32_t events_per_shard,
                                       const std::uint32_t max_payload_bytes,
                                       const std::uint32_t max_session_count,
                                       const EventQueueFullPolicy full_policy,
                                       CloseRequest close_request)
    : impl_(std::make_unique<Impl>(shard_count, events_per_shard,
                                   max_payload_bytes, max_session_count,
                                   full_policy, std::move(close_request))) {}

QueuedEventAdapter::~QueuedEventAdapter() = default;

void QueuedEventAdapter::OnConnected(const SessionId session) {
  if (!impl_->Push(QueuedEventHeader{.type = QueuedEventType::kConnected,
                                     .session = session},
                   {}) &&
      impl_->full_policy == EventQueueFullPolicy::kDisconnect &&
      impl_->close_request) {
    impl_->close_request(session);
  }
}

std::size_t QueuedEventAdapter::OnData(const SessionId session,
                                       const std::span<const std::byte> data) {
  if (impl_->Push(
          QueuedEventHeader{.type = QueuedEventType::kData, .session = session},
          data)) {
    return data.size();
  }
  if (impl_->full_policy == EventQueueFullPolicy::kDisconnect &&
      impl_->close_request) {
    impl_->close_request(session);
  }
  return 0;
}

void QueuedEventAdapter::OnClosed(const SessionId session,
                                  const CloseReason reason) {
  static_cast<void>(
      impl_->Push(QueuedEventHeader{.type = QueuedEventType::kClosed,
                                    .session = session,
                                    .close_reason = reason},
                  {}));
}

PollEventResult QueuedEventAdapter::TryPop(
    const std::uint32_t shard_index, const std::span<std::byte> destination,
    QueuedEventHeader& header) noexcept {
  if (shard_index >= impl_->shards.size()) {
    return PollEventResult::kEmpty;
  }
  Impl::Shard& shard = *impl_->shards[shard_index];
  std::scoped_lock lock(shard.mutex);
  if (shard.count == 0) {
    return PollEventResult::kEmpty;
  }
  const Impl::Slot& slot = shard.slots[shard.head];
  if (destination.size() < slot.header.payload_bytes) {
    header = slot.header;
    return PollEventResult::kDestinationTooSmall;
  }
  header = slot.header;
  if (header.payload_bytes != 0) {
    std::memcpy(destination.data(), slot.payload.data(), header.payload_bytes);
  }
  const auto wait = std::chrono::steady_clock::now() - slot.enqueued_at;
  const auto wait_nanoseconds = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(wait).count());
  shard.head = (shard.head + 1) % shard.slots.size();
  --shard.count;
  impl_->depth.fetch_sub(1, std::memory_order_relaxed);
  impl_->dequeued.fetch_add(1, std::memory_order_relaxed);
  impl_->wait_total_nanoseconds.fetch_add(wait_nanoseconds,
                                          std::memory_order_relaxed);
  UpdateMaximum(impl_->wait_max_nanoseconds, wait_nanoseconds);
  return PollEventResult::kEvent;
}

std::uint32_t QueuedEventAdapter::shard_count() const noexcept {
  return static_cast<std::uint32_t>(impl_->shards.size());
}

std::uint64_t QueuedEventAdapter::EstimatedMemoryBytes() const noexcept {
  return impl_->estimated_memory_bytes;
}

MetricsSnapshot QueuedEventAdapter::metrics() const noexcept {
  return MetricsSnapshot{
      .event_queue_high_watermark =
          impl_->high_watermark.load(std::memory_order_relaxed),
      .event_queue_rejections = impl_->rejected.load(std::memory_order_relaxed),
      .event_queue_depth = impl_->depth.load(std::memory_order_relaxed),
      .event_queue_dequeued = impl_->dequeued.load(std::memory_order_relaxed),
      .event_queue_wait_total_nanoseconds =
          impl_->wait_total_nanoseconds.load(std::memory_order_relaxed),
      .event_queue_wait_max_nanoseconds =
          impl_->wait_max_nanoseconds.load(std::memory_order_relaxed),
  };
}

}  // namespace rionetlite
