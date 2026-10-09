# 공개 API 레퍼런스

| 헤더 | 주요 API |
|---|---|
| `config.h` | `ServerConfig`, `ValidateServerConfig`, worker policy와 메모리 예산 |
| `error.h` | `Error`, `SendResult`, `CloseReason`, 안정된 문자열 변환 |
| `event_handler.h` | `SessionId`, `EventHandler`, 구조화 `Logger` |
| `server.h` | move-only `Server`, lifecycle, send/close/metrics |
| `packet_decoder.h` | 안전한 big-endian length-prefix decoder |
| `queued_event_adapter.h` | shard 유계 이벤트 전달과 포화 정책 |
| `metrics.h` | 인스턴스별 누적 metric snapshot |
| `version.h` | SemVer compile-time macro |

공개 C++ 타입과 함수는 `rionetlite` namespace에 있다. thread safety와 span lifetime은 각 헤더 주석 및 [스레딩 문서](../architecture.md#스레드와-콜백)를 따른다. `RIONetLite/` 경로는 전환 호환 계층이며 신규 코드에서 사용하지 않는다.

## 함수별 계약

공개 namespace는 rionetlite다. 신규 코드는 include/rionetlite만 사용한다. 아래 표는 호출 계약이며 symbol 목록은 이 문서 아래에 있다. 다른 스레드에서 객체를 이동·파괴하려면 모든 애플리케이션 호출자가 빠져나와야 한다. handler/logger도 Server보다 오래 살아야 한다.

| 함수·signature | 사전 조건·스레드 | 수명·결과·실패 |
|---|---|---|
| `ValidateServerConfig(const ServerConfig&) -> expected<ValidatedServerConfig, Error>` | 자원 확보 전 검증이다. 독립 호출 가능하다. | 범위·CQ·page·overflow를 확인한다. 실패 field를 읽는다. 계산 예산은 실제 process 메모리가 아니다. |
| `DefaultIoWorkerCount() noexcept -> uint32_t` | processor group을 반영한다. | max_sessions를 줄이면 worker 수도 그 이하로 지정한다. |
| `Server::Create(ServerConfig, EventHandler&, Logger*=nullptr)` | 실행할 주 스레드에서 호출한다. | 성공은 kInitialized다. 즉시 bind·자원 확보하며 설정·Winsock·RIO·allocation 오류를 expected로 보고한다. 포트 실패는 native_code를 보존한다. |
| `Server::Start() -> expected<void, Error>` | kInitialized에서 한 번이다. 같은 서버 event/logger callback에서는 거절한다. | accept/I/O 시작이다. kRunning은 시작 중에도 관측될 수 있다. 중간 실패 시 rollback 후 kStopped다. 실패한 객체를 재시작하지 않고 새로 Create한다. |
| `Server::Stop(StopMode=kGraceful) noexcept` | kInitialized/kRunning 및 반복 종료 가능하다. 동시 호출은 같은 결과를 기다린다. event/logger callback은 kInvalidState다. | kStopped 뒤 재시작 불가다. Send/RequestClose 진입자를 배출한 뒤 join한다. 시간 초과·CQ 오류는 실패다. [종료](../operations.md#정상-종료)에서 deadline의 한계를 확인한다. |
| `Send(SessionId, span<const byte>) noexcept` | kRunning이다. 다른 Send/RequestClose/Stop과 동시 호출 가능하다. | 반환 전에 복사한다. empty도 kMessageTooLarge다. 아래 결과표를 따른다. |
| `Send(SessionId, header span, payload span) noexcept` | 위와 같다. | 두 span 합계가 message maximum 이하다. header와 payload가 하나의 수락 단위다. |
| `RequestClose(SessionId, CloseReason=kApplicationRequest) noexcept -> bool` | kRunning이다. callback에서 호출 가능하다. | 비동기 close 전이 승자는 true다. stale/이미 closing/비실행은 false다. application/protocol/overload/idle 원인을 지정할 수 있으며 나머지 지정값은 application으로 매핑된다. |
| `state() const noexcept` | 동시 조회 가능하다. | 순간 snapshot이며 다음 API 성공을 보장하지 않는다. moved-from은 kStopped다. |
| `Metrics() const noexcept` | 동시 조회 가능하다. | 누적 counter의 eventually consistent snapshot이다. adapter 지표는 별도 metrics다. enable_metrics=false면 모두 0이다. |
| Server move/destructor | callback 내부에서 파괴하지 않는다. 모든 application 호출자 종료가 선행한다. | destructor가 미종료 transport를 정리한다. 명시적 Stop으로 오류를 먼저 확인한다. |

| SendResult | 의미와 대응 |
|---|---|
| kQueued | 복사·수락이다. RIO 완료나 상대 ACK가 아니다. |
| kWouldBlock | send 링 공간 부족이다. 유계 retry/상위 rate limit/close를 선택한다. |
| kMessageTooLarge | empty 또는 합계 크기 초과다. 입력을 수정한다. |
| kStaleSession | index/generation이 현재 연결과 다르다. 이전 작업을 버린다. |
| kClosing | 서버나 세션이 종료 중이다. 새 작업을 만들지 않는다. |
| kUnavailable | 전송 게시 등 내부 실패다. 오류 로그와 close를 확인한다. |

| callback | 실행 위치·계약 |
|---|---|
| `OnConnected(SessionId)` | accept 스레드다. 다른 세션의 I/O callback과 동시 실행 가능하다. 같은 세션의 OnClosed가 반환 전에 추월하지 않는다. 예외는 격리·기록한다. |
| `OnData(SessionId, span<const byte>) -> size_t` | 해당 세션의 소유 I/O 워커다. 소비량은 0..span.size다. span 저장 금지다. 예외·초과 소비는 protocol close다. |
| `OnClosed(SessionId, CloseReason)` | 소유 I/O 워커다. 같은 수명에서 한 번이며 이후 OnData가 없다. 예외는 격리·기록한다. |
| `Logger::IsEnabled(LogSeverity) const noexcept` | 여러 호출 스레드에서 실행될 수 있다. 짧게 반환하고 예외를 던지지 않는다. |
| `Logger::Log(LogSeverity,string_view)` 및 `Log(const LogEvent&)` | lifecycle/accept/I/O 등 동시 실행 가능하다. 저장하려면 string_view 내용을 복사한다. Log 예외는 격리한다. blocking file write는 별도 logging queue로 넘긴다. |

| adapter/decoder 함수 | 계약 |
|---|---|
| QueuedEventAdapter constructors | shard/events/payload/session 양수다. kDisconnect에는 CloseRequest가 필수다. invalid_argument 또는 allocation 예외가 가능하다. 명시적인 max_session_count는 서버와 맞춘다. 생략 overload는 shard×events를 session capacity로 사용한다. |
| adapter OnConnected/OnData/OnClosed | EventHandler 구현이다. raw stream은 max_payload_bytes 이하 조각으로 복사·소비한다. 정책은 [배압](../operations.md#배압과-유계-저장소)을 따른다. |
| `TryPop(shard, destination, header) noexcept` | 동시 producer/consumer 가능하며 짧은 shard mutex를 사용한다. kEvent면 destination이 owned copy다. kDestinationTooSmall은 제거 없이 필요한 크기를 header에 적는다. 잘못된 shard는 kEmpty다. |
| `shard_count()`, `EstimatedMemoryBytes()`, `metrics()` | 고정 설정/추정 또는 atomic snapshot이다. process 전체 메모리나 kernel pinned bytes를 뜻하지 않는다. |
| `LengthPrefixedDecoder(max_packet_bytes)` | minimum header 크기 이상 maximum을 사용한다. 생성 자체는 noexcept다. |
| `Decode(span, handler) -> DecodeResult` | immutable decoder를 공유할 수 있지만 handler의 공유 상태는 사용자가 보호한다. span 전체에서 완성 packet만 callback한다. span 수명과 handler 예외는 호출자 책임이다. 자체 누적 없음, consumed_bytes/packet_count/error를 반환한다. |
| `ToString(ErrorCategory/ErrorCode/SendResult/CloseReason) noexcept` | 진단 문자열 변환이다. | 반환 string_view를 수정하지 않는다. |

대표 회귀 근거는 public_helpers_test, chat_protocol_test, worker_control_test와 실제 RIO integration의 bind/native error·callback reentry·throwing logger·stale slot·동시 Stop·parked close·반복 handle 회수 검사다. 단위 검사가 전체 OS·사용자·운영 환경 검증을 대신하지 않는다.

<!-- BEGIN GENERATED API -->
## 공개 symbol 목록

> `tools\generate-api-docs.ps1`이 공개 헤더의 `@api` 주석에서 생성한다.

| 헤더 | symbol | 설명 |
|---|---|---|
| `config.h` | `DefaultIoWorkerCount` | processor group을 반영한 기본 worker 수다. |
| `config.h` | `ServerConfig` | 서버 생성 전 사용자가 지정하는 설정이다. |
| `config.h` | `ValidatedServerConfig` | 검증된 설정과 계산된 자원 예산이다. |
| `config.h` | `ValidateServerConfig` | 설정 범위와 메모리/CQ 관계를 검증한다. |
| `config.h` | `WorkerWaitMode` | RIO worker의 대기 전략이다. |
| `error.h` | `CloseReason` | 세션이 닫힌 원인을 나타낸다. |
| `error.h` | `Error` | 라이브러리·native 오류와 진단 문맥이다. |
| `error.h` | `ErrorCategory` | 오류가 발생한 계층을 분류한다. |
| `error.h` | `ErrorCode` | 공개 API의 안정된 오류 코드다. |
| `error.h` | `SendResult` | 비동기 송신 요청의 즉시 결과다. |
| `error.h` | `ToString` | 공개 enum을 안정된 문자열로 변환한다. |
| `event_handler.h` | `EventHandler` | 네트워크 수명·수신 이벤트 callback 계약이다. |
| `event_handler.h` | `LogEvent` | 세션·작업·native 오류를 포함한 로그 이벤트다. |
| `event_handler.h` | `Logger` | 서버 인스턴스별 구조화 로그 sink다. |
| `event_handler.h` | `LogSeverity` | 구조화 로그의 심각도다. |
| `event_handler.h` | `SessionId` | 슬롯 인덱스와 재사용 세대를 결합한 세션 식별자다. |
| `metrics.h` | `MetricsSnapshot` | 서버와 event queue의 누적 계측 snapshot이다. |
| `packet_decoder.h` | `DecodeError` | length-prefix decode 실패 원인이다. |
| `packet_decoder.h` | `DecodeResult` | 소비 바이트와 decode 오류를 반환한다. |
| `packet_decoder.h` | `LengthPrefixedDecoder` | big-endian length-prefix stream decoder다. |
| `queued_event_adapter.h` | `EventQueueFullPolicy` | event queue 포화 시 대기 없이 거절하는 정책이다. |
| `queued_event_adapter.h` | `PollEventResult` | event dequeue 시도 결과다. |
| `queued_event_adapter.h` | `QueuedEventAdapter` | callback을 shard별 유계 queue로 전달한다. |
| `queued_event_adapter.h` | `QueuedEventHeader` | queue event의 세션·순서·payload 메타데이터다. |
| `queued_event_adapter.h` | `QueuedEventType` | 유계 queue에 저장되는 이벤트 종류다. |
| `server.h` | `Server` | move-only RIO TCP 서버 인스턴스다. |
| `server.h` | `ServerState` | 공개 서버 수명 상태다. |
| `server.h` | `StopMode` | graceful 또는 immediate 종료 정책이다. |
<!-- END GENERATED API -->
