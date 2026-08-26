# ADR 0005: 성능 측정 방법

- 상태: 승인
- 날짜: 2026-08-26

## 결정

- Release x64와 symbol을 사용한다.
- 장비, OS, 전원 정책, 커밋, worker/CQ/buffer 설정, client 수, payload와 시간을 결과에 기록한다.
- warm-up, measurement, cool-down을 분리하고 같은 조합을 최소 5회 실행한다.
- 처리량과 함께 P50/P95/P99 latency, CPU, context switch, queue high-watermark, 오류와 데이터 불일치를 기록한다.
- 반복 결과의 중앙값을 대표값으로 사용한다.
- correctness counter가 0이 아닌 결과는 성능 결과로 인정하지 않는다.
- 기준선 이후 처리량 5% 이상 하락 또는 P99 10% 이상 악화를 기본 조사 조건으로 한다. 장비 분산이 큰 CI 결과는 자동 실패 대신 추세로 사용한다.

## 비교 규칙

한 번에 worker count, batch size, polling mode, allocator 또는 buffer 전략 중 하나만 바꾼다. polling과 절전 모드는 별도 결과로 보고한다. 패킷당 allocation 0회를 정상 상태 목표로 둔다.
