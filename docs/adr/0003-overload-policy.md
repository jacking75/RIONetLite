# ADR 0003: 과부하와 백프레셔 정책

- 상태: 승인
- 날짜: 2026-08-26

## 결정

모든 큐와 버퍼는 설정 가능한 상한을 가진다. 네트워크 worker를 애플리케이션 생산자나 소비자가 block하지 않는다.

| 자원 | 포화 시 동작 |
|---|---|
| 세션 풀 | 새 연결을 즉시 거절하고 counter를 증가시킨다. |
| 세션 recv buffer | 새 recv 게시를 중단하고 protocol/receive overflow 사유로 연결을 종료한다. |
| 세션 send queue | 데이터를 쓰지 않고 `SendResult::kWouldBlock`을 반환한다. |
| application event queue | 기본값은 해당 세션 종료다. 선택 정책은 newest reject를 허용하지만 blocking은 허용하지 않는다. |
| RIO CQ | 시작 전에 설정에서 요구 용량을 검증하고 생성 실패를 반환한다. corrupt CQ는 fatal server error로 승격한다. |

## 이유

무제한 큐는 순간 부하를 메모리 고갈과 긴 tail latency로 바꾼다. 게임 서버에서는 손실 정책과 연결 종료가 명시적인 편이 시스템 전체 장애보다 낫다.

## 결과

- 각 포화 결과는 오류 코드, 로그와 메트릭으로 구분한다.
- 기본 용량은 시작 시 예상 메모리에 포함한다.
- T10과 T11이 실제 유계 큐를 구현하기 전까지 신규 API는 legacy 송신 실패를 `kUnavailable`로 보수적으로 매핑한다.

## 2026-10-09 현행 구현 보충

본문의 T 단계는 당시 계획의 이력이다. 현재 계약은 [함수별 API](../api/contracts.md), [배압](../backpressure.md), [종료](../graceful-shutdown.md)를 따른다. callback Start/Stop은 거절한다. adapter는 짧은 mutex와 유계 raw chunk 저장소이며 RejectNewest는 소비·폐기 정책이다. close는 session mailbox로 합치고 parked close도 depth에 포함한다. Stop은 단일 deadline을 사용하되 사용자 callback 중단과 추가 safety drain의 wall-clock 상한을 보장하지 않는다.
