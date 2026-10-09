#pragma once

#include "rionetlite/event_handler.h"
#include "rionetlite/metrics.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>

namespace rionetlite {

/// @api QueuedEventType - 유계 queue에 저장되는 이벤트 종류다.
enum class QueuedEventType : std::uint8_t { kConnected, kData, kClosed };
/// @api EventQueueFullPolicy - event queue 포화 시 대기 없이 거절하는 정책이다.
enum class EventQueueFullPolicy : std::uint8_t {
  // Lossy raw chunks; unsuitable for framed TCP streams. Use kDisconnect there.
  kRejectNewest,
  kDisconnect,
};

/// @api QueuedEventHeader - queue event의 세션·순서·payload 메타데이터다.
struct QueuedEventHeader final {
  QueuedEventType type = QueuedEventType::kConnected;
  SessionId session{};
  CloseReason close_reason = CloseReason::kNone;
  std::uint64_t sequence = 0;
  std::size_t payload_bytes = 0;
};

/// @api PollEventResult - event dequeue 시도 결과다.
enum class PollEventResult : std::uint8_t {
  kEvent,
  kEmpty,
  kDestinationTooSmall,
};

/// @api QueuedEventAdapter - callback을 shard별 유계 queue로 전달한다.
class QueuedEventAdapter final : public EventHandler {
 public:
  using CloseRequest = std::function<void(SessionId)>;

  // Dimensions must be positive; kDisconnect requires close_request. Invalid
  // arguments throw std::invalid_argument. Shard access uses a short mutex.
  // OnData splits raw streams into max_payload_bytes chunks and consumes them.

  QueuedEventAdapter(std::uint32_t shard_count, std::uint32_t events_per_shard,
                     std::uint32_t max_payload_bytes,
                     EventQueueFullPolicy full_policy,
                     CloseRequest close_request = {});
  QueuedEventAdapter(std::uint32_t shard_count, std::uint32_t events_per_shard,
                     std::uint32_t max_payload_bytes,
                     std::uint32_t max_session_count,
                     EventQueueFullPolicy full_policy,
                     CloseRequest close_request = {});
  ~QueuedEventAdapter() override;

  QueuedEventAdapter(const QueuedEventAdapter&) = delete;
  QueuedEventAdapter& operator=(const QueuedEventAdapter&) = delete;

  void OnConnected(SessionId session) override;
  std::size_t OnData(SessionId session,
                     std::span<const std::byte> data) override;
  void OnClosed(SessionId session, CloseReason reason) override;

  [[nodiscard]] PollEventResult TryPop(std::uint32_t shard,
                                       std::span<std::byte> destination,
                                       QueuedEventHeader& header) noexcept;
  [[nodiscard]] std::uint32_t shard_count() const noexcept;
  [[nodiscard]] std::uint64_t EstimatedMemoryBytes() const noexcept;
  [[nodiscard]] MetricsSnapshot metrics() const noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace rionetlite
