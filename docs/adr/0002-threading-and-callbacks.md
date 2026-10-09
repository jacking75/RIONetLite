# ADR 0002: 스레드와 콜백 계약

- 상태: 승인
- 날짜: 2026-08-26

## 결정

- `Server::Send`, `Server::RequestClose`, `Server::state`는 여러 애플리케이션 스레드에서 호출할 수 있다.
- `Start`와 `Stop`은 내부 상태 전이로 직렬화한다. 여러 스레드의 중복 `Stop`은 안전하고 같은 종료 결과를 관찰해야 한다.
- 직접 `EventHandler`는 내부 네트워크 스레드에서 호출한다.
- `OnConnected`는 accept 경로에서 호출하고, 동일 세션의 `OnData`와 `OnClosed`는 그 세션의 소유 I/O worker에서 호출한다.
- 사용자는 callback에서 게임 로직, DB, 파일 I/O, sleep과 blocking lock을 실행하지 않아야 한다.
- callback으로 전달한 data span은 callback이 반환될 때까지만 유효하다. 보관하려면 사용자가 복사해야 한다.
- 한 `SessionId`에 대해 `OnConnected`가 먼저 관찰되고 `OnClosed`는 최대 한 번 관찰되어야 한다.
- 같은 세션의 `OnData`와 `OnClosed`는 동시에 호출하지 않는다. 이 보장은 T06~T11에서 상태 머신과 event adapter로 완성한다.
- 서로 다른 세션 callback은 동시에 호출될 수 있다.
- callback 안에서 `Send`와 `RequestClose`를 호출할 수 있다. callback 재진입으로 다른 callback을 동기 호출하지 않는다.

## 권장 사용

실제 게임 서버는 T11의 `QueuedEventAdapter`를 사용하여 네트워크 callback에서 소유 메시지를 유계 큐로 넘기고 즉시 반환한다. direct handler는 최소 지연이 필요하고 수명 규칙을 직접 관리할 수 있는 사용자에게 제공한다.

## 결과

- 공개 API 문서에는 함수별 thread safety와 view lifetime을 표기한다.
- callback 수행 시간과 애플리케이션 큐 깊이를 T14 메트릭에 포함한다.

## 2026-10-09 현행 구현 보충

본문의 T 단계는 당시 계획의 이력이다. 현재 계약은 [함수별 API](../api/contracts.md), [배압](../backpressure.md), [종료](../graceful-shutdown.md)를 따른다. callback Start/Stop은 거절한다. adapter는 짧은 mutex와 유계 raw chunk 저장소이며 RejectNewest는 소비·폐기 정책이다. close는 session mailbox로 합치고 parked close도 depth에 포함한다. Stop은 단일 deadline을 사용하되 사용자 callback 중단과 추가 safety drain의 wall-clock 상한을 보장하지 않는다.
