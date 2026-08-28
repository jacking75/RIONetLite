# 성능·안정성 정밀 점검과 개선 계획 (2026-08-28)

0.2.0 릴리스 후보(`RIONetLite/impl/`, `src/`, `include/rionetlite/`) 전체 소스를 정밀 리뷰한 결과다. 고성능 게임 서버 용도를 기준으로 개선이 필요한 항목, 그 근거와 정확한 구현 방법을 우선순위와 함께 정리한다. 기존 벤치마크 결정(ADR 0005, `docs/performance-tuning.md`)과 중복되는 항목은 제외했다.

## 요약

| ID | 분류 | 제목 | 심각도 | 난이도 |
|---|---|---|---|---|
| S1 | 안정성 | `FastSpinlock`이 스핀락이 아니라 `Sleep`+`timeBeginPeriod` 락이다 | 높음 | 낮음 |
| S2 | 안정성 | 송신 실패 시 index 단독 `ForcedClose`가 세대 검증 불변 조건을 위반한다 | 높음 | 낮음 |
| S3 | 안정성 | 종료 후 지연 수신 완료가 `OnData`를 `OnClosed` 이후에 전달할 수 있다 | 높음 | 낮음 |
| S4 | 안정성 | 비-드레인 강제 종료 경로의 자원 해제 순서가 커널 미완료 I/O와 충돌한다 | 높음 | 중간 |
| S5 | 안정성 | `QueuedEventAdapter`가 큐 포화 시 `kClosed` 이벤트를 유실한다 | 중간 | 중간 |
| S6 | 안정성 | 사용자 콜백 예외가 I/O 워커를 `std::terminate`시킨다 | 중간 | 낮음 |
| S7 | 안정성 | peer FIN 후 드레인에 상한 시간이 없다(좀비 드레이닝 세션) | 중간 | 중간 |
| S8 | 안정성 | accept 실패 시 백오프가 없어 오류 폭주 시 CPU를 소진한다 | 중간 | 낮음 |
| S9 | 안정성 | listen 소켓 종료와 accept 루프 사이의 데이터 레이스 | 낮음 | 낮음 |
| S10 | 안정성 | CQ 용량을 `RIO_MAX_CQ_SIZE`와 비교하지 않는다 | 낮음 | 낮음 |
| P1 | 성능 | 초기 free list가 워커 0에 세션을 몰아준다(cold-start 편중) | 높음 | 낮음 |
| P2 | 성능 | 송신 슬롯 구조를 바이트 링 코얼레싱으로 전환한다 | 높음 | 높음 |
| P3 | 성능 | 다음 수신 게시를 콜백 이전으로 이동한다(수신 파이프라이닝) | 중간 | 중간 |
| P4 | 성능 | 메트릭 핫패스 비용(워커 역산 나눗셈, QPC 2회)을 줄인다 | 낮음 | 낮음 |
| P5 | 성능 | 종료 시 전 세션 스캔에 early-exit을 넣는다 | 낮음 | 낮음 |
| F1 | 기능 | keepalive/idle timeout이 없어 죽은 클라이언트가 슬롯을 점유한다 | 높음 | 중간 |
| F2 | 기능 | 헤더+본문 scatter `Send` API(선택) | 낮음 | 낮음 |

---

## A. 안정성 개선

### S1. `FastSpinlock`을 SRWLOCK으로 교체한다

**현상** — `RIONetLite/impl/fast_spin_lock.h:33-44`의 `EnterLock`은 경합 즉시 `timeBeginPeriod(1)` → `Sleep(min(10, nloops))` → `timeEndPeriod(1)`을 반복한다.

- 첫 경합 반복에서 이미 `Sleep(0)`으로 타임슬라이스를 양보하고, 반복마다 시스템 전역 타이머 해상도를 바꾸는 syscall을 2회 수행한다.
- 이 락은 가장 뜨거운 경로에 있다: `SendImmediately::PostSend`/`Completed`(앱 스레드 ↔ I/O 워커가 세션마다 상시 경합), `SessionManager::AllocClientSession`/`ReturnSession`(accept 스레드 ↔ 워커).
- 결과적으로 송신 경합 한 번에 최소 수 µs~수 ms의 지연 스파이크가 생기고, `timeBeginPeriod` 남발은 전체 시스템 전력/스케줄링에 영향을 준다. `Sleep(0)`은 우선순위가 같은 스레드에만 양보하므로 우선순위 역전 시 최대 10ms씩 잠들며 회복한다.

**구현 방법** — `FastSpinlock`의 구현을 SRWLOCK으로 바꾼다. 인터페이스(`EnterLock`/`LeaveLock`)와 `FastSpinlockGuard`는 유지하므로 호출부 수정이 없다.

```cpp
// fast_spin_lock.h
#include <windows.h>

class FastSpinlock {
 public:
  FastSpinlock() = default;
  ~FastSpinlock() = default;
  FastSpinlock(const FastSpinlock&) = delete;
  FastSpinlock& operator=(const FastSpinlock&) = delete;

  void EnterLock() noexcept { AcquireSRWLockExclusive(&lock_); }
  void LeaveLock() noexcept { ReleaseSRWLockExclusive(&lock_); }

 private:
  SRWLOCK lock_ = SRWLOCK_INIT;
};
```

SRWLOCK은 비경합 시 인터락드 1회, 경합 시 내부적으로 짧은 스핀 후 커널 대기로 전환되어 현재 요구(짧은 임계 구역, 소유자 없음, 비재귀)에 정확히 맞는다. `mmsystem.h`/`timeBeginPeriod` 의존과 winmm 링크도 함께 제거한다. 직접 스핀을 유지하고 싶다면 test-and-test-and-set + `_mm_pause` + 일정 횟수 후 `WaitOnAddress`(C++20 `atomic::wait`) 구조로 바꿔야 하지만, 검증 비용 대비 이득이 없어 SRWLOCK을 권장한다.

**검증** — 단위 테스트는 그대로 통과해야 한다. `tools\run-benchmark.ps1`로 latency 시나리오 P99와 context switch를 전후 비교한다. 다중 앱 스레드가 동일 세션에 `Send`를 경합시키는 스트레스 케이스를 통합 테스트에 추가한다.

### S2. 송신 실패 경로의 index 단독 `ForcedClose`를 세대 검증 경로로 바꾼다

**현상** — `RIONetLite/impl/server_net.h:143-145`에서 `SendData`가 `kSEND_IO_FAIL`을 반환하면 private `ForcedClose(session_index, reason)`(191행)를 호출한다. 이 오버로드는 index만으로 `GetSessionObj(session_index)`를 조회해 무조건 `SetDisableNetwork()`를 호출하고 **현재** UID를 읽어 지연 종료 큐에 넣는다.

실패 시점과 `ForcedClose` 실행 사이에 해당 슬롯이 재활용되어 새 연결이 들어오면 무고한 새 세션을 끊는다. CLAUDE.md의 불변 조건("인덱스만으로 지연된 송신 완료나 연결 종료를 처리하지 않는다")을 위반하는 유일한 경로다.

**구현 방법** — private 오버로드를 제거하고, 호출부에서 원래 요청에 쓰인 `session_uid`를 그대로 전달해 세대 검증이 있는 public `ForcedClose`를 사용한다.

```cpp
// server_net.h  ServerNetManagerImpl::SendData
if (auto ret = session_mgr_.SendData(session_index, session_uid, data_size, p_data);
    ret != NetErrorCode::kNONE) {
  if (ret == NetErrorCode::kSEND_IO_FAIL) {
    static_cast<void>(
        ForcedClose(session_index, session_uid, DisconnectReason::kSEND_ERROR));
  }
  return ret;
}
```

public `ForcedClose`는 `SessionManager::ForcedClose → GetSessionObj(index, uid)`로 UID 불일치 시 아무것도 하지 않으므로 재활용 레이스가 사라진다. `ForcedClose(const std::int32_t, DisconnectReason)` 오버로드(191-199행)는 삭제한다.

**검증** — 단위 테스트: fake RIO로 `Send` 실패를 주입한 뒤 슬롯을 재활용시키고, 새 세대 세션이 끊기지 않음을 확인한다.

### S3. 종료된 세션의 지연 수신 완료가 콜백을 재전달하지 않게 한다

**현상** — `server_net_io_worker.h:246-253`의 성공 수신 완료 경로는 `IsConnected()`를 확인하지 않는다. 앱 요청 종료(`ForcedClose` → lazyed 큐 → `Disconnect` → `OnClose` 전달 → closesocket) 직전에 커널이 CQ에 성공 완료를 넣어둔 경우, 다음 배치에서 UID가 아직 일치하므로(outstanding이 남아 재활용 불가) `OnReceive`가 **`OnClosed` 이후에** 호출된다. "Connect → Receive → Close 관찰 순서 일관" 계약 위반이다. `QueuedEventAdapter`는 gate로 걸러주지만 handler를 직접 쓰는 사용자에게 그대로 노출된다.

**구현 방법** — `ProcessRIOCQ`의 수신 성공 분기 진입 시 연결 상태를 먼저 확인한다.

```cpp
} else if (IOType::kRECV == p_context->io_type_) {
  if (!p_session->IsConnected()) {
    // 종료가 먼저 관찰된 세션의 지연 완료다. 콜백 없이 정리만 한다.
    p_session->RecvFailed();
    return;
  }
  if (!RecvCompletion(worker, p_session, transferred)) { ... }
```

`RecvFailed()`는 `recv_context_.Fail()`과 outstanding 감소를 수행하므로 세션 재활용 조건이 정상적으로 충족된다. 송신 완료는 이미 `SendCompletion`이 `IsConnected()`를 확인하므로 추가 수정이 없다.

**검증** — 통합 테스트: 데이터 수신 직후 앱 스레드에서 `RequestClose`를 호출하는 반복 테스트에서 `OnClosed` 이후 `OnData`가 관찰되지 않음을 단언한다(handler에 순서 기록 추가).

### S4. 비-드레인 강제 종료 경로의 해제 순서를 바로잡는다

**현상** — `server_net.h:112-117`의 `End`는 워커가 타임아웃되면(`drained_ == false`) 다음 순서로 해제한다.

1. `rio_completion_queues_.clear()` — 미완료 요청이 남은 상태에서 CQ를 닫는다.
2. `session_mgr_.AbandonOutstanding()` — outstanding 카운터를 0으로 강제한다.
3. `session_mgr_.Dispose()` — 이때 비로소 소켓을 닫고, `RegisteredRioBuffer::Reset()`이 (카운터가 0이 되었으므로) 커널 요청이 남아 있는 버퍼를 deregister하고, `DoubleMappedBuffer` 소멸자가 매핑을 해제한다.

커널이 아직 참조 중일 수 있는 CQ/등록 버퍼/매핑을 순서대로 회수하므로, 이 경로에 들어가면 프로세스 내 메모리 손상(AV 또는 무증상 오염)이 가능하다. `AbandonOutstanding`이 RAII 보호 장치(`Reset`은 outstanding≠0이면 거부)를 스스로 무력화하는 구조다.

**구현 방법** — "소켓 먼저 닫기 → 취소 완료를 짧게 드레인 → 그래도 남으면 자원을 의도적으로 누수"로 바꾼다.

```cpp
if (!drained_) {
  // 1) 모든 소켓을 먼저 닫아 커널이 미완료 요청을 취소하게 만든다.
  for (std::uint32_t i = 0; i < max; ++i) {
    if (auto* s = session_mgr_.GetSessionObj(i); s != nullptr) {
      s->AbortSocket();  // socket_.Reset()만 수행하는 새 메서드
    }
  }
  // 2) 취소 완료를 이 스레드에서 짧게 드레인한다(워커는 이미 종료됨).
  const auto drain_deadline = steady_clock::now() + 500ms;
  std::vector<RIORESULT> results(net_config_.completion_batch_size_);
  bool all_zero = false;
  while (!all_zero && steady_clock::now() < drain_deadline) {
    for (auto& cq : rio_completion_queues_) {
      const ULONG n = rio_api_.DequeueCompletion(cq.get(), results.data(),
                                                 results.size());
      for (ULONG i = 0; n != RIO_CORRUPT_CQ && i < n; ++i) {
        auto* ctx = reinterpret_cast<RioIoContext*>(results[i].RequestContext);
        if (auto* s = session_mgr_.GetSessionObj(ctx->session_index_)) {
          ctx->io_type_ == IOType::kRECV ? s->RecvFailed() : s->SendFailed(ctx);
        }
      }
    }
    all_zero = session_mgr_.Outstanding() == 0;
  }
  // 3) 그래도 남으면 등록 버퍼·매핑·CQ를 해제하지 않고 의도적으로 누수한다.
  if (!all_zero) {
    session_mgr_.LeakRegisteredResources();  // RegisteredRioBuffer/DoubleMappedBuffer release()
    for (auto& cq : rio_completion_queues_) cq.Release();  // 새 메서드: 닫지 않고 소유권 포기
  }
}
session_mgr_.Dispose();
rio_completion_queues_.clear();
```

필요한 보조 변경: `TcpSession::AbortSocket()`, `UniqueRioCompletionQueue::Release()`, `RegisteredRioBuffer`/`DoubleMappedBuffer`에 `Release()`(소유권 포기) 추가, `SessionManager::AbandonOutstanding` 제거(카운터 조작 대신 위 드레인으로 실제 0을 만든다). 누수를 택한 경우 `remaining_outstanding_`과 함께 로그로 보고한다. 이 경로는 이미 실패 반환(`kShutdownTimedOut`)이므로 API 변화는 없다.

**검증** — 통합 테스트: fake RIO에서 완료를 영원히 반환하지 않는 시나리오로 `Stop` 타임아웃을 만들고, ASan 빌드에서 UAF가 없음을 확인한다. 정상 드레인 경로 회귀는 기존 shutdown 테스트로 커버된다.

### S5. `QueuedEventAdapter`의 `kClosed` 이벤트 유실을 없앤다

**현상** — `src/queued_event_adapter.cpp:125-131`에서 shard가 가득 차면 `kClosed`도 gate만 갱신하고 이벤트 자체는 버린다(rejected 카운트만 증가). 게임 서버 입장에서는 `OnClosed`를 끝내 받지 못하므로 플레이어 객체·룸 참가 상태가 영구히 새는 치명적 경로다. 종료 이벤트는 유계 큐의 드롭 대상이 되어서는 안 된다.

**구현 방법** — 세대당 최대 1개인 점을 이용해 close 이벤트에 별도의 보장 저장소를 둔다. 순서 보장(해당 세션의 Data를 모두 소비한 뒤 Closed 전달)을 위해 세션별 in-ring 카운터를 함께 둔다.

1. `SessionGate`에 필드 추가:

```cpp
struct SessionGate final {
  std::uint64_t generation = 0;
  std::uint64_t next_sequence = 0;
  std::uint32_t in_ring = 0;          // 이 세션의 이벤트가 링에 몇 개 있는가
  bool connected = false;
  bool closed = false;
  bool close_parked = false;          // 링 포화로 보류된 close
  CloseReason parked_reason = CloseReason::kNone;
};
```

2. `Push`: 정상 경로에서 `++gate.in_ring`. `kClosed`인데 링이 가득 차면 드롭 대신 `close_parked = true; parked_reason = ...; gate = next_gate;` 후 `true`를 반환한다(호출자가 disconnect 정책을 또 타지 않게).
3. shard에 보류 close 전달용 대기열 추가: `std::vector<std::uint32_t> parked_sessions;`(shard mutex로 보호, push는 `close_parked` 설정 시).
4. `TryPop`: 이벤트를 하나 꺼낼 때마다 해당 세션 gate의 `--in_ring`. 꺼낸 뒤 `in_ring == 0 && close_parked`이면 다음 `TryPop` 호출에서 링보다 먼저 parked close를 합성해 반환한다(`header.type = kClosed`, `payload_bytes = 0`, `sequence = gate.next_sequence++`). shard가 비어 있을 때도 `parked_sessions`를 먼저 확인한다.
5. 같은 세션의 다음 세대 `kConnected`는 기존 gate 교체 로직상 parked 상태를 덮어쓸 수 있으므로, `kConnected` 수락 조건에 `!gate.close_parked`를 추가해 소비자가 이전 세대의 Closed를 보기 전에는 새 세대를 받지 않게 한다(생산자는 `false`를 받고 `close_request` 정책을 따른다).

**검증** — 단위 테스트: shard 용량 1로 만들고 Data로 채운 뒤 Closed를 push → TryPop 순서가 Data, Closed임을 단언한다. 포화 상태에서 세대 교체 시나리오도 추가한다.

### S6. 사용자 콜백 예외를 격리한다

**현상** — `src/server.cpp`의 `OnConnect`/`OnReceive`/`OnClose` 브리지는 `handler_` 호출을 예외 방어 없이 수행한다. 사용자 handler가 예외를 던지면 I/O 워커(또는 accept 스레드) 스택을 뚫고 나가 `std::terminate`로 서버 전체가 죽는다.

**구현 방법** — 세 브리지에서 handler 호출을 try/catch로 감싼다. `OnData` 예외는 프로토콜 오류와 동일하게 세션을 끊고 0을 반환한다. `OnConnected`/`OnClosed` 예외는 로그만 남긴다.

```cpp
// Impl::OnReceive 내부
std::size_t consumed = 0;
try {
  consumed = handler_.OnData(session_id, bytes);
} catch (const std::exception& e) {
  Emit(LogSeverity::kError, "session", session_id, "receive_callback",
       ErrorCode::kInternalError, 0, e.what());
  static_cast<void>(legacy_server_.ForcedClose(
      index, generation, DisconnectReason::kPROTOCOL_ERROR));
  return 0;
} catch (...) { /* 동일 처리, 메시지는 "unknown exception" */ }
```

`docs/threading-model.md`에 "콜백에서 벗어난 예외는 세션 종료로 처리된다"를 명시한다.

**검증** — 단위 테스트: 예외를 던지는 handler로 세션이 `kProtocolError`로 닫히고 서버가 계속 동작함을 확인한다.

### S7. peer FIN 드레인에 상한 시간을 둔다

**현상** — 0바이트 수신(kZERO_COMPLETION) 시 `DisConnect`(`server_net.h:177-180`)는 `ReadyForPeerClose()`(송신 outstanding 0)가 될 때까지 종료를 보류하고, `ProcessLazyedDisConnectSession`이 매 루프 재큐잉한다. peer가 FIN 직후 사라지면(전원 차단 등) 미완료 송신이 TCP 재전송 타임아웃(수 분)까지 완료되지 않아, 그동안 세션 슬롯이 드레이닝 상태로 잠기고 polling 모드에서는 재큐잉이 busy 루프가 된다.

**구현 방법** — `LazyedDisConnectSession`에 마감 시각을 추가하고, 초과 시 abortive로 강등한다.

```cpp
// define.h
struct LazyedDisConnectSession {
  std::int32_t session_index_ = -1;
  std::uint64_t uid = 0;
  DisconnectReason reason_ = DisconnectReason::kNONE;
  std::int64_t drain_deadline_ns_ = 0;  // 0이면 미설정
};
```

`ProcessLazyedDisConnectSession`에서 `DisConnectFunc`가 `false`를 반환해 처음 보류될 때 `drain_deadline_ns_ = now + peer_close_drain_timeout_ms`를 채워 재큐잉하고, 이미 마감이 지난 항목은 `reason_`을 `kSEND_ERROR` 계열(abortive)로 바꿔 다시 시도한다. abortive `Disconnect`는 closesocket으로 미완료 송신을 취소하므로 즉시 드레인된다. 설정은 `ServerConfig::peer_close_drain_timeout_ms`(기본 5000, `ToLegacyConfig`로 전달)로 노출하고 `ValidateServerConfig`에 범위 검사(1..60000)를 추가한다.

**검증** — 통합 테스트는 실제 peer 소멸 재현이 어려우므로 fake RIO로 송신 완료를 지연시키는 단위 수준 검증 + LoadClient에 "FIN 후 즉시 종료" 시나리오를 추가해 슬롯이 timeout 내 회수되는지 확인한다.

### S8. accept 실패 백오프를 추가한다

**현상** — `server_net_accepter.h:53-63`에서 `accept` 실패 시 `OnRejectedConnection`만 호출하고 즉시 재시도한다. 핸들 고갈(WSAEMFILE)·비페이지풀 부족 같은 지속 실패 상황에서 accept 스레드가 100% CPU로 돌며 로그를 폭주시킨다.

**구현 방법** — 연속 실패 카운터 기반 지수 백오프를 넣는다.

```cpp
std::uint32_t consecutive_failures = 0;
while (is_run_...) {
  detail::UniqueSocket accepted_socket(accept(...));
  if (!accepted_socket) {
    if (!is_run_...) break;
    net_event_mgr->OnRejectedConnection(...);
    ++consecutive_failures;
    Sleep(std::min<DWORD>(100, consecutive_failures));  // 1ms→100ms 상한
    continue;
  }
  consecutive_failures = 0;
  ...
}
```

세션 풀 고갈(`AllocClientSession == nullptr`) 분기에도 동일한 백오프를 적용하면 accept 폭주 시 SYN 큐가 자연스럽게 배압 역할을 한다.

**검증** — 단위 테스트로는 fake socket api에 연속 실패를 주입해 sleep 호출(주입 가능한 sleeper)이 증가하는지 확인한다.

### S9. listen 소켓 종료 레이스를 정리한다

**현상** — `Accepter::Stop`은 다른 스레드에서 `listen_socket_->Reset()`으로 소켓을 닫아 blocked `accept`를 깨운다(Windows 관행). 그러나 `Run` 루프는 매 반복 `listen_socket.get()`을 다시 읽으므로 비원자 SOCKET 멤버에 대한 형식적 데이터 레이스가 있고, 닫힌 핸들 값이 재사용될 이론적 여지가 있다.

**구현 방법** — `Run` 진입 시 한 번만 캐시한다.

```cpp
void Run(...) {
  const SOCKET listen_handle = listen_socket.get();  // 루프 전 1회 캐시
  while (is_run_.load(std::memory_order_acquire)) {
    detail::UniqueSocket accepted_socket(accept(listen_handle, nullptr, nullptr));
    ...
```

소켓 생성은 accept 스레드만 수행하므로 캐시된 핸들 값이 다른 소켓으로 재사용될 경로가 없고, close 이후 `accept`는 `WSAENOTSOCK`으로 즉시 반환되어 `is_run_` 확인 후 종료한다.

**검증** — 기존 즉시 Start/Stop 반복 테스트로 회귀를 확인한다.

### S10. CQ 용량 상한을 검증 단계로 옮긴다

**현상** — `src/config.cpp:129-134`는 계산된 CQ 용량을 `ULONG` 상한과만 비교한다. RIO의 실제 상한은 `RIO_MAX_CQ_SIZE`(0x8000000)이며, 초과 시 `Start`에서 `kIO_THREAD_START_FAIL`이라는 불친절한 오류로 실패한다.

**구현 방법** — 검증 조건을 교체한다.

```cpp
if (completion_capacity > RIO_MAX_CQ_SIZE) {
  return Invalid("max_outstanding_sends_per_session",
                 "calculated RIO completion queue capacity exceeds RIO_MAX_CQ_SIZE");
}
```

`config.cpp`가 `MSWSock.h`를 포함하게 되면 `WinSock2.h` 선행 포함 규칙을 지킨다(값 `0x8000000`을 상수로 정의해 Windows 헤더 추가를 피하는 방법도 가능).

**검증** — 단위 테스트: 상한 초과 설정이 `kInvalidConfiguration`으로 거부되는지 확인한다.

---

## B. 성능 개선

### P1. 세션 free list를 워커 간 인터리브한다

**현상** — `session_manager.h:32-58`은 워커 0의 파티션 전체 → 워커 1 전체 순서로 free list를 만들고, `AllocClientSession`은 항상 앞에서 꺼낸다. 서버 기동 직후 최초 N개의 동시 접속(N = 워커당 세션 수)이 **전부 워커 0**에 배정된다. 8워커·세션 1024 구성이면 첫 128 접속이 한 스레드의 CQ에 몰려 cold-start 처리량이 워커 1개 수준으로 떨어진다. churn이 쌓여야 서서히 섞인다.

**구현 방법(1차, 필수)** — 초기 free list를 라운드로빈으로 채운다. `PrepareSessionPool`에서 세션 생성은 지금처럼 하되, free list 삽입만 분리한다.

```cpp
// 세션 생성 루프에서는 free_session_indexs_에 넣지 않는다.
// 생성 완료 후:
std::vector<detail::SessionPartition> partitions(rioThreadCount);
for (std::int32_t i = 0; i < rioThreadCount; ++i) {
  partitions[i] = detail::PartitionSessions(net_cfg.max_session_count_,
                                            rioThreadCount, i);
}
for (std::uint32_t j = 0;; ++j) {
  bool pushed = false;
  for (std::int32_t i = 0; i < rioThreadCount; ++i) {
    if (j < partitions[i].count) {
      free_session_indexs_.emplace_back(
          SessionIndexContext{.index_ = partitions[i].first_index + j});
      pushed = true;
    }
  }
  if (!pushed) break;
}
```

**구현 방법(2차, 선택)** — 정상 운영 중에도 편중이 남으면 워커별 free deque + `std::atomic<uint32_t> leased_per_worker_[]`를 두고 `AllocClientSession`에서 최소 부하 워커의 deque에서 꺼낸다. 워커 수는 최대 256이므로 선형 최소 탐색으로 충분하다. 1차 적용 후 벤치마크로 필요성을 판단한다.

**검증** — 단위 테스트: 순차 `AllocClientSession` 결과의 `GetRioThreadId()`가 0,1,2,...로 순환하는지 단언한다. 벤치마크 churn/burst 시나리오에서 워커별 처리 편차를 확인한다.

### P2. 송신을 슬롯 배열에서 바이트 링 코얼레싱으로 전환한다

**현상** — `send_immediately.h`는 `max_outstanding_sends`개의 고정 슬롯(각 `max_message_bytes` 크기)을 두고 패킷 1개당 RIOSend 1회를 게시한다.

- 20바이트 이동 패킷도 16KiB 슬롯 하나를 통째로 소비한다. 소형 패킷 버스트는 버퍼 바이트가 남아돌아도 슬롯 수(기본 16) 초과 즉시 `kWouldBlock`이 된다.
- 패킷마다 RIOSend·완료 이벤트가 1개씩 발생해 CQ 압력과 워커 dequeue 비용이 패킷 수에 비례한다.
- CQ 용량도 `세션 × (1 + max_outstanding_sends)`로 커진다.
- 슬롯 탐색이 락 안에서 O(슬롯 수) 선형 스캔이다(97-104행).

**구현 방법** — 세션당 in-flight 송신을 1개로 고정하고, 링에 쌓인 바이트를 완료 시점에 하나의 `RIO_BUF`로 묶어 보낸다. `RingBufferVA2`가 이미 double-mapped라 wrap 구간도 연속 주소로 게시할 수 있어 구조가 단순해진다.

`SendImmediately`를 다음 상태로 재구성한다(락은 S1의 SRWLOCK).

```cpp
// 상태: send_context_ 1개, ring(write_pos/read_pos/used), in_flight(bool)
NetErrorCode PostSend(uint64_t uid, uint32_t size, const char* data) {
  FastSpinlockGuard guard(lock_);
  if (!accepting_posts_ || uid != active_session_uid_) return ...;
  if (size > ring_size_ - used_) return NetErrorCode::kSEND_THREAD_BUFFER_FULL;
  std::memcpy(ring_.data() + write_pos_, data, size);  // double map이 wrap 흡수
  write_pos_ = (write_pos_ + size) % ring_size_;
  used_ += size;
  if (in_flight_) return NetErrorCode::kNONE;          // 완료가 이어서 보낸다
  return PostPendingLocked();                          // 아래 참조
}

NetErrorCode PostPendingLocked() {  // lock 보유 상태에서 호출
  const uint32_t len = used_;      // 항상 <= ring_size_ 이므로 연속 게시 가능
  send_context_.BeginRequest(active_session_uid_, send_rio_buffer_.get(),
                             read_pos_, len, ring_size_ * 2);
  send_rio_buffer_.AddOutstanding();
  in_flight_ = true;
  posts_in_flight_.fetch_add(1, std::memory_order_acq_rel);
  // RIOSend는 기존과 동일하게 락 밖에서 호출하고 실패 시 Failed 처리한다.
}

bool Completed(RioIoContext* ctx, uint32_t transferred) {
  FastSpinlockGuard guard(lock_);
  if (!ctx->CompleteSend(transferred)) { FailUnlocked(ctx); return false; }
  read_pos_ = (read_pos_ + transferred) % ring_size_;
  used_ -= transferred;
  send_rio_buffer_.CompleteOutstanding();
  in_flight_ = false;
  if (used_ != 0 && accepting_posts_) {
    return PostPendingLocked() == NetErrorCode::kNONE;  // 부분 송신 잔여+신규를 한 번에
  }
  return true;
}
```

동반 변경:

- `Accepter::NewSession`의 `CreateRequestQueue` 인자에서 `max_outstanding_send`를 `1`로 바꾼다(`server_net_accepter.h:109-113`).
- `ValidateServerConfig`의 CQ 용량 계산을 `sessions_per_worker * 2`로 바꾸고, `send_buffer_bytes >= max_message_bytes` 제약으로 완화한다(슬롯 수 × 메시지 크기 제약 삭제). `max_outstanding_sends_per_session`은 의미가 사라지므로 제거하거나 "링에 대기 가능한 바이트"로 문서를 바꾼다.
- `SendCompletion`/`Failed`/`BeginClose`/`AbandonOutstanding`은 컨텍스트가 1개가 되어 단순해진다. `posts_in_flight_` 게이트는 그대로 유지한다.
- 부분 송신은 `CompleteSend`가 이미 Offset/Length를 전진시키므로 위 코드에서 자연 처리된다. 단 `BeginRequest`를 다시 쓰는 형태이므로 `CompleteSend` 후 `Length != 0`인 잔여를 새 `BeginRequest`로 합치는 위 방식이 더 단순하다(컨텍스트를 idle로 만들고 다시 게시).
- 완료 1회가 여러 패킷을 대표하므로 `sent_bytes` 메트릭은 그대로, `send_operations`는 "RIOSend 횟수"로 의미를 갱신한다.

**효과** — 소형 패킷 위주 게임 트래픽에서 RIOSend/완료 이벤트 수가 "패킷 수"에서 "burst 수"로 줄고, 송신 배압이 슬롯이 아닌 바이트 기준이 되어 `kWouldBlock` 빈도가 급감한다. CQ 용량 축소로 메모리도 준다. broadcast가 많은 ChatServer 시나리오에서 효과가 가장 크다.

**검증** — 부분 송신, 원격 종료, 큐 포화, 강제 종료 단위 테스트를 새 경로로 갱신한다. 벤치마크 throughput/burst/fan-in 시나리오 전후 비교가 필수다. 이 항목은 공개 설정 의미가 바뀌므로 README·`docs/backpressure.md`·`docs/configuration.md`·예제를 함께 갱신한다.

### P3. 다음 수신 게시를 콜백 앞으로 옮긴다

**현상** — `server_net_io_worker.h:246-253`은 수신 완료 → `OnReceive` 콜백 → 소비 → `PostRecv` 순서다. 콜백(QueuedEventAdapter 복사 등)이 도는 동안 NIC이 이 세션의 데이터를 쓸 수 없어 세션당 수신이 왕복 지연에 묶인다.

**구현 방법** — 링에 여유가 있으면 콜백 전에 다음 수신을 게시한다. 커널은 `[write_pos, write_pos+len)`에 쓰고 콜백은 `[read_pos, read_pos+used)`를 읽으므로 겹치지 않는다(double map, 같은 워커 스레드만 완료를 처리).

```cpp
} else if (IOType::kRECV == p_context->io_type_) {
  if (!p_session->RecvCompletion(transferred)) { ...오류 처리 기존과 동일... }
  // 1) 여유가 있으면 먼저 게시한다(kRECV_BUFFER_FULL은 여기서 끊지 않는다).
  bool posted = false;
  if (auto r = p_session->PostRecv(); r == NetErrorCode::kNONE) {
    posted = true;
  } else if (r != NetErrorCode::kRECV_BUFFER_FULL) {
    DisConnectFunc(...kIO_REQUEST_RECV_ERROR...); return;
  }
  // 2) 콜백과 소비.
  if (!InvokeReceiveCallback(worker, p_session)) {  // 기존 RecvCompletion의 콜백 부분 분리
    DisConnectFunc(..., DisconnectReason::kPROTOCOL_ERROR); return;
  }
  // 3) 1)에서 여유가 없었다면 소비 후 재시도. 그래도 0이면 기존 정책대로 끊는다.
  if (!posted) { PostRecv(cq_session_uid, p_session); }
}
```

기존 `RecvCompletion` 헬퍼는 "완료 반영"과 "콜백+소비"(`InvokeReceiveCallback`)로 분리한다. 콜백이 프로토콜 오류로 종료를 반환한 경우 이미 게시된 수신은 closesocket이 취소하므로 추가 처리가 없다(S3 적용 시 지연 완료도 안전하다).

**검증** — 기존 프레이밍·버퍼 포화 테스트 통과 + latency 시나리오 P50/P99 전후 비교. `max_receive_chunk_bytes < 메시지 크기`인 분할 수신 케이스에서 순서 보존을 재확인한다.

### P4. 메트릭 핫패스 비용을 줄인다

**현상** — `src/server.cpp:224-238`의 `MetricsForSession`은 수신·송신·종료마다 나눗셈 2회로 워커를 역산하고, 콜백마다 `steady_clock::now()`(QPC)를 2회 호출한다. 소형 패킷 고빈도에서 패킷당 수십 ns가 고정 비용으로 붙는다.

**구현 방법**

1. `Impl` 생성자에서 `std::vector<std::uint16_t> session_worker_lut_`를 `PartitionSessions`와 동일한 규칙으로 한 번 채우고, `MetricsForSession`은 `worker_metrics_[session_worker_lut_[index]]` 조회로 바꾼다.
2. `ServerConfig`에 `enable_callback_timing`(기본 true)을 추가해 QPC 측정을 별도 게이트로 분리한다. false면 `callback_*` 카운터만 비활성화되고 나머지 메트릭은 유지된다.

**검증** — 메트릭 스냅샷 단위 테스트 갱신, observability off/on 벤치 비교(기존 harness 재사용).

### P5. 종료 대기 스캔에 early-exit을 넣는다

**현상** — 워커 루프 조건(`server_net_io_worker.h:142-144`)은 종료 중 매 반복 `Outstanding()`(전 세션 합산)과 `ActiveCount()`(전 세션 순회)를 호출한다. 세션 수가 크면 종료 구간에서 워커들이 전 세션의 원자 변수를 반복 스캔한다.

**구현 방법** — `SessionManager`에 bool 버전을 추가하고 루프 조건에서 사용한다.

```cpp
bool AnyOutstandingOrActive() const noexcept {
  for (const auto& session : sessions_) {
    if (session->Outstanding() != 0 || session->HasOpenSocket()) return true;
  }
  return false;
}
```

첫 활성 세션에서 즉시 반환하므로 평균 비용이 크게 준다. `End`의 보고용 `remaining_*` 집계는 기존 함수를 유지한다.

**검증** — 기존 shutdown 단위·통합 테스트로 충분하다.

---

## C. 게임 서버 관점 기능 추가

### F1. keepalive와 idle timeout

**현상** — 죽은 클라이언트(단말 크래시, NAT 타임아웃)는 아무 I/O도 일으키지 않으므로 현재 구조에서는 영원히 세션 슬롯과 등록 버퍼를 점유한다. 수신 대기만 있는 게임 로비 서버에서 슬롯 고갈로 이어진다.

**구현 방법** — 두 단계로 넣는다.

1. **TCP keepalive**: `TcpSession::OnConnect`에서 `WSAIoctl(SIO_KEEPALIVE_VALS)`를 설정한다.

```cpp
#include <mstcpip.h>
tcp_keepalive keepalive{.onoff = 1,
                        .keepalivetime = cfg.keepalive_time_ms,      // 예: 30'000
                        .keepaliveinterval = cfg.keepalive_interval_ms};  // 예: 5'000
DWORD bytes = 0;
WSAIoctl(socket.get(), SIO_KEEPALIVE_VALS, &keepalive, sizeof(keepalive),
         nullptr, 0, &bytes, nullptr, nullptr);
```

죽은 피어는 keepalive 실패 → 미완료 수신이 오류로 완료 → 기존 오류 경로로 자동 정리된다.

2. **애플리케이션 idle timeout(선택)**: `TcpSession`에 `std::atomic<std::uint64_t> last_activity_ms_`를 두고 `RecvCompletion` 성공 시 `GetTickCount64()`를 relaxed로 기록한다. `IoWorkerThread`가 약 1초마다(`GetTickCount64() >= next_sweep_ms` 비교) 자기 파티션(`PartitionSessions(max, workers, thread_index)`) 범위만 순회해 `IsConnected() && now - last_activity > idle_timeout_ms`인 세션을 `lazyed_dis_conn_mgr_->AddSession(..., DisconnectReason::kIDLE_TIMEOUT)`으로 넘긴다. `DisconnectReason`에 `kIDLE_TIMEOUT`, `CloseReason`에 `kIdleTimeout`을 추가한다.

설정: `ServerConfig`에 `keepalive_time_ms`(0=off), `keepalive_interval_ms`, `idle_timeout_ms`(0=off)를 추가하고 검증 범위를 정한다. 파티션 단위 스윕은 워커당 세션 수에 비례하고 1초 주기이므로 핫패스 영향이 없다.

**검증** — 통합 테스트: idle_timeout을 짧게 설정한 뒤 무통신 클라이언트가 `kIdleTimeout`으로 닫히는지, 활동 중 클라이언트는 유지되는지 확인한다.

### F2. 헤더+본문 scatter `Send` API(선택)

앱이 길이 프리픽스를 붙이려고 매번 임시 버퍼에 조립하는 복사를 없앤다. P2(바이트 링) 적용 후에는 자연스럽게 구현된다.

```cpp
SendResult Send(SessionId session, std::span<const std::byte> header,
                std::span<const std::byte> payload) noexcept;
```

내부는 링에 header, payload를 연속 memcpy 2회로 넣는 것뿐이다(락 1회 유지). 합계 크기로 기존 검증을 수행한다. EchoServer/ChatServer 예제와 README를 함께 갱신한다.

---

## D. 검토 후 제외한 항목

- **`RIO_MSG_DEFER` + `RIO_MSG_COMMIT_ONLY` 배칭** — 현재(그리고 P2 이후) 구조는 RQ당 배치 내 게시가 최대 1건이라 커밋 절감분이 없다. RQ당 다건 게시 구조가 생기기 전에는 도입하지 않는다.
- **`AcceptEx` 전환** — 현 accept 스레드는 초당 수백 접속 수준까지 병목이 아니다. churn 벤치마크에서 accept가 병목으로 측정되기 전에는 복잡도를 추가하지 않는다(S8 백오프만 적용).
- **worker arena 일괄 버퍼 등록** — 2026-08-27 설계 벤치에서 시작 시간 64% 단축이 확인됐지만 세션 격리·실패 롤백 이점으로 보류가 이미 결정됐다(`docs/performance-tuning.md`). 대규모 세션 구성에서 시작 시간이 문제로 측정될 때 재개한다.
- **mimalloc 기본 활성화** — 측정상 이득이 없어 기존 결정(off)을 유지한다.
- **수신 버퍼 포화 시 read-pause 정책** — 소비하지 않는 handler와 결합하면 교착이 되므로, 현행 `kRECV_BUFFER_FULL` 종료 정책이 더 안전하다. 유지한다.

## 권장 적용 순서

1. **1차(저위험 수정 묶음)**: S1, S2, S3, S6, S8, S9, S10, P1, P5 — 코드 변경이 국소적이고 공개 API가 바뀌지 않는다. 한 번에 적용 후 전체 검증을 돌린다.
2. **2차(동작 정책 추가)**: S7, F1, P4 — 설정 필드가 추가되므로 README·configuration 문서 갱신이 따른다.
3. **3차(구조 변경)**: S4, P3, S5 — 각각 독립적으로 적용하고 벤치마크/ASan을 반드시 통과시킨다.
4. **4차(송신 재설계)**: P2, F2 — 가장 효과가 크지만 설정 의미가 바뀐다. 별도 브랜치에서 benchmark 회귀 게이트(처리량 -5%, P99 +10%)로 판정한다.

각 단계의 공통 검증 명령은 다음과 같다.

```powershell
tools\build.ps1 -Configuration Debug -Rebuild
tools\build.ps1 -Configuration Release -Rebuild
clang-format --dry-run --Werror --style=file <변경 파일>
tools\run-benchmark.ps1   # 성능 관련 항목 적용 시
```
