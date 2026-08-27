# ?앹꽦??怨듦컻 API 紐⑸줉

> `tools\generate-api-docs.ps1`??怨듦컻 ?ㅻ뜑??`@api` 二쇱꽍?먯꽌 ?앹꽦?쒕떎.

| ?ㅻ뜑 | symbol | ?ㅻ챸 |
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
| `queued_event_adapter.h` | `EventQueueFullPolicy` | event queue 포화 시 비차단 정책이다. |
| `queued_event_adapter.h` | `PollEventResult` | event dequeue 시도 결과다. |
| `queued_event_adapter.h` | `QueuedEventAdapter` | callback을 shard별 유계 queue로 전달한다. |
| `queued_event_adapter.h` | `QueuedEventHeader` | queue event의 세션·순서·payload 메타데이터다. |
| `queued_event_adapter.h` | `QueuedEventType` | 유계 queue에 저장되는 이벤트 종류다. |
| `server.h` | `Server` | move-only RIO TCP 서버 인스턴스다. |
| `server.h` | `ServerState` | 공개 서버 수명 상태다. |
| `server.h` | `StopMode` | graceful 또는 immediate 종료 정책이다. |
