# Changelog

## 0.3.0 - 2026-08-28

과거 2026-08-28 변경 이력이다. 이후 공개 준비 수정은 아래와 같으며 version 0.3.0의 개발 후보에 반영했다. 새 release tag 생성·공개를 뜻하지 않는다.

### 2026-10-09 공개 준비 수정

- Logger.Log 예외 격리, event/logger callback의 Start/Stop 거절과 transport API 호출자 수명 보호를 추가했다.
- generation 검사와 close 전이를 pool lock으로 보호하고 close mailbox·batch를 유계화했다. accept의 callback·첫 recv 게시 순서를 보호했다.
- adapter raw split·손실 RejectNewest 계약·parked close depth·유효 생성 인자를 수정했다.
- Chat의 다중 shard broadcast와 .NET 10 C# 클라이언트를 version 1 protocol과 공유 fixture로 맞췄다.
- native bind 오류·idle close reason·성공 RIO 완료 sent_bytes를 수정했다. 수락 누적 바이트는 새 queued_send_bytes다.
- worker startup/null completion 방어와 Stop 단일 deadline, CQ 종료 후 notification handle 해제를 추가했다. 강제 safety drain에서 heap allocation을 없앴다.
- stress는 모든 경로 예열 후 quiescent 자원을 비교하며 불충분 표본·비정상 exit·강제 종료를 실패로 판정한다.
- 패키지는 현재 source snapshot에서 fresh Release /MD non-/GL 빌드를 만들고 metadata/hash·source·최종 ZIP-only 소비 검사를 제공한다.
- 설치·입문·API·Chat·운영·영문 핵심 안내와 보안 연락 경로를 확장했다.

이 수정은 MetricsSnapshot 레이아웃과 sent_bytes 의미를 바꾸므로 0.x 바이너리를 함께 다시 빌드해야 한다. kRejectNewest는 소비·폐기하는 raw 손실 정책이며 framed TCP에는 kDisconnect를 사용한다. 문서의 함수 계약과 릴리스 체크리스트에서 검증 범위를 확인한다.

### 안정성

- 세션 송신과 세션 풀이 쓰던 락을 SRWLOCK으로 교체했다. 이전 구현은 경합마다 시스템 타이머 해상도를 바꾸고 `Sleep`으로 양보해 지연 스파이크를 만들었다.
- 송신 실패로 인한 강제 종료가 세션 generation을 함께 검증한다. 실패와 종료 사이에 슬롯이 재사용돼도 새 연결을 끊지 않는다.
- 종료를 먼저 관찰한 세션의 지연 수신 완료를 콜백 없이 정리한다. `OnClosed` 이후 `OnData`가 전달되지 않는다.
- 종료 timeout 경로의 자원 회수 순서를 바로잡았다. 소켓을 먼저 닫아 커널이 요청을 취소하게 하고 취소 완료를 다시 drain하며, 그래도 남으면 등록 버퍼·매핑·CQ를 해제하지 않고 소유권만 포기한다.
- `QueuedEventAdapter`가 큐 포화 시에도 `kClosed`를 유실하지 않는다. 보류했다가 해당 세션의 남은 이벤트를 모두 소비한 뒤 순서대로 전달한다.
- 사용자 콜백에서 벗어난 예외를 격리한다. `OnData` 예외는 프로토콜 오류로 세션을 닫고, `OnConnected`/`OnClosed` 예외는 기록만 한다.
- peer FIN 이후 송신 drain에 `peer_close_drain_timeout_ms` 상한을 두고, 초과하면 abortive close로 강등해 슬롯을 회수한다.
- accept 실패와 세션 풀 고갈에 상한 100 ms의 지수 백오프를 적용했다.
- accept 루프가 listen 소켓 핸들을 루프 진입 시 한 번만 읽는다.
- RIO completion queue 용량을 `RIO_MAX_CQ_SIZE` 기준으로 시작 전에 검증한다.

### 성능

- 세션 free list를 worker 파티션에 걸쳐 라운드로빈으로 구성했다. 기동 직후 첫 접속들이 worker 0에 몰리지 않는다.
- 송신을 고정 슬롯 배열에서 바이트 링 코얼레싱으로 전환했다. 세션당 in-flight 요청이 1개로 고정되고, 대기 중인 바이트를 하나의 `RIO_BUF`로 묶어 게시한다. 완료 이벤트 수가 패킷 수가 아니라 burst 수에 비례하며 CQ 용량이 세션당 2개로 줄었다.
- 다음 수신 게시를 수신 콜백보다 먼저 수행한다.
- 세션→worker 조회 테이블을 미리 만들어 hot path의 나눗셈을 없앴고, 콜백 시간 계측을 `enable_callback_timing`으로 분리했다.
- 종료 대기 조건이 전체 세션 합산 대신 첫 활성 세션에서 즉시 반환한다.

### 기능

- `keepalive_time_ms`와 `idle_timeout_ms`를 추가했다. 무통신 상태로 사라진 피어가 세션 슬롯을 영구히 점유하지 않는다. 새 종료 원인은 `CloseReason::kIdleTimeout`이다.
- 헤더와 본문을 임시 버퍼 없이 한 메시지로 보내는 `Server::Send(session, header, payload)` 오버로드를 추가했다.

### 호환성

- `ServerConfig::max_outstanding_sends_per_session`을 제거했다. 송신 배압은 `send_buffer_bytes`가 결정한다.
- `CloseReason`에 `kIdleTimeout`이 추가되어 enum 값이 재배치됐다.
- `RIONetLite/server_net.h` 호환 API는 이번 릴리스에도 유지하며 제거 대상 릴리스는 0.4.0으로 미룬다.

## 0.2.0 - 2026-08-27

- `include/rionetlite` 공개 API, generation 기반 `SessionId`, 유계 송수신 및 이벤트 큐를 추가했다.
- polling/adaptive/notification RIO worker 정책, 구조화 로그와 메트릭을 추가했다.
- EchoServer, shard 기반 ChatServer, IOCP LoadClient와 단위·통합·스트레스·벤치마크 자동화를 추가했다.

이 프로젝트는 0.x 동안 ABI 호환을 보장하지 않는다. 공개 API 폐기는 최소 한 minor 릴리스 동안 deprecated 상태를 거친다.
