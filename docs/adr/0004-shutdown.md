# ADR 0004: 종료 정책

- 상태: 승인
- 날짜: 2026-08-26

## 결정

`StopMode`은 graceful과 immediate 두 모드를 제공한다.

- graceful은 새 accept와 외부 send를 막고 이미 수락된 send 및 close event를 설정된 timeout까지 처리한다.
- immediate는 새 I/O 게시를 막고 socket을 닫은 뒤 취소/오류 completion만 drain한다.
- timeout 뒤에는 immediate 단계로 전환하며 남은 session과 outstanding request 수를 결과와 로그에 남긴다.
- 종료 순서는 accept 중단 → session close 요청 → completion drain → worker join → CQ/registered buffer 해제 → Winsock 해제다.
- `Stop`은 idempotent하며 소멸자에서 예외를 던지지 않는다.

## 단계적 적용

T03은 API와 상태 전이를 제공한다. T05는 초기화 실패의 RAII rollback을 제공한다. outstanding completion을 포함한 전체 graceful drain은 T08에서 완성한다. 그 전에는 graceful 요청을 현재 가능한 안전한 immediate 종료로 처리하고 진단 가능하게 유지한다.

## 2026-10-09 현행 구현 보충

본문의 T 단계는 당시 계획의 이력이다. 현재 계약은 [함수별 API](../api/contracts.md), [배압](../backpressure.md), [종료](../graceful-shutdown.md)를 따른다. callback Start/Stop은 거절한다. adapter는 짧은 mutex와 유계 raw chunk 저장소이며 RejectNewest는 소비·폐기 정책이다. close는 session mailbox로 합치고 parked close도 depth에 포함한다. Stop은 단일 deadline을 사용하되 사용자 callback 중단과 추가 safety drain의 wall-clock 상한을 보장하지 않는다.
