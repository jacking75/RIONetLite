# 성능 튜닝

기본값은 logical processor에서 한 코어의 애플리케이션 여유를 남긴 worker 수, batch 64, adaptive 128 spin이다. polling은 최고 부하에서 지연을 낮출 수 있지만 idle 코어를 소비하고, notification은 idle 효율이 좋으며, adaptive는 둘의 절충이다. affinity는 NUMA와 processor group을 측정한 뒤에만 켠다.

한 번에 worker, batch(32/64/128/256), wait mode, buffer, allocator 중 하나만 바꾸고 `tools\run-benchmark.ps1` 결과를 비교한다. correctness error가 0이 아닌 결과를 성능 개선으로 채택하면 안 된다. mimalloc은 최종 executable이 선택적으로 링크하며 RIO 등록 메모리 소유와 무관하다.

## 2026-08-27 짧은 기준 측정

20 logical processor 장비, Release, 8개 연결, 128-byte payload, 조합별 1초×5회 중앙값이다. 모든 데이터 오류는 0이었다. 짧은 로컬 측정이므로 릴리스용 고정 장비 결과를 대신하지 않는다.

| wait mode | batch | median packets | median P99 µs |
|---|---:|---:|---:|
| polling | 32 / 64 / 128 / 256 | 26,601 / 33,485 / 33,165 / 36,943 | 437 / 348 / 445 / 342 |
| adaptive | 32 / 64 / 128 / 256 | 51,869 / 51,513 / 50,206 / 51,467 | 288 / 282 / 298 / 275 |
| notification | 32 / 64 / 128 / 256 | 52,636 / 52,610 / 52,208 / 54,105 | 280 / 290 / 277 / 284 |

기본 adaptive/64는 notification에 가까운 지연·처리량을 유지하면서 burst 시 spin 여지를 남기는 보수적 절충이다. polling은 기본 worker 수가 많은 이 장비에서 CPU 경쟁으로 오히려 악화됐으므로 기본값으로 선택하지 않았다.

`tools\test-mimalloc.ps1`은 동일 ChatServer create/start/graceful-stop 수명 검증과 Echo 부하를 allocator off/on으로 각각 빌드·실행한다.

2026-08-27 로컬 Release 비교에서는 32개 연결, 128바이트 payload, 2초 측정을 각각 3회 실행했다. 중앙값은 기본 allocator가 37,420 packets/s와 P99 1,509us, mimalloc이 28,179 packets/s와 P99 2,576us였으며 양쪽 모두 데이터 오류와 누락이 0이었다. 짧은 측정에서 mimalloc의 이점이 없으므로 기본값은 비활성으로 유지한다. 원본 JSON/CSV는 `benchmarks/results/<timestamp>-mimalloc/`에 생성된다.

## 2026-08-27 공식 시나리오 초기 기준선

20 logical processor 장비의 Release, adaptive/64, observability off에서 각 시나리오를 warm-up 1초 후 2초×5회 측정했다. 45회 모두 connection/data/dropped/timeout counter가 0이었다. 이는 이 장비의 초기 로컬 기준선이며 승인된 고정 릴리스 장비 회귀선은 아니다.

| scenario | median packets/s | median P99 µs | median server CPU |
|---|---:|---:|---:|
| latency 32 B | 47,160 | 171 | 4.882% |
| latency 128 B | 47,065 | 181 | 4.882% |
| throughput 1 KiB | 40,992 | 1,504 | 4.845% |
| throughput 4 KiB | 30,077 | 2,071 | 4.805% |
| throughput 16 KiB | 13,200 | 4,094 | 4.882% |
| fan-in, 128 connections × 10pps | 194 | 764,840 | 0.118% |
| burst | 45,034 | 2,463 | 4.882% |
| slow consumer | 42,170 | 1,430 | 4.805% |
| churn | 17,494 | 1,700 | 3.360% |

같은 날 32연결·128-byte·1초×3회 wait/batch 매트릭스를 context-switch 성능 카운터와 함께 다시 측정했다. 36회 모두 정확성 오류가 0이었다.

| wait mode | batch 32 / 64 / 128 / 256 median packets/s | median P99 µs | median server CPU | median context switches/s |
|---|---:|---:|---:|---:|
| polling | 24,916 / 28,462 / 29,331 / 28,399 | 7,590 / 6,079 / 5,779 / 7,269 | 691.56 / 604.295 / 602.345 / 625.31% | 5,056.974 / 5,752.086 / 5,505.324 / 5,187.016 |
| adaptive | 43,712 / 40,684 / 43,953 / 44,782 | 1,252 / 1,199 / 1,158 / 1,204 | 5 / 5 / 5 / 5% | 363.878 / 372.794 / 365.526 / 363.83 |
| notification | 41,183 / 43,224 / 43,268 / 43,182 | 1,293 / 1,215 / 1,212 / 1,213 | 5 / 4.92 / 4.92 / 5% | 346.603 / 338.025 / 341.205 / 343.032 |

polling은 이 20 logical processor 장비에서 worker가 경쟁해 처리량과 P99가 모두 나쁘고 context switch도 adaptive의 약 14~16배다. notification은 유휴 CPU를 최소화하고 adaptive는 burst에서 제한된 spin 여지를 유지하면서 notification과 유사한 CPU·지연을 보인다. 따라서 게임 로직용 CPU 여유와 지연을 함께 고려해 adaptive/64를 기본값으로 유지한다. WPR ETW는 정책 오류 `0xc5585011`로 차단됐으며 스레드별 원인 분석이 필요한 승인 환경에서만 추가 수집한다.

## 2026-08-27 버퍼 등록·송신 준비·affinity 비교

20 logical processor 장비의 Release에서 `tools\run-design-benchmark.ps1`을 실행했다. 256세션, 세션당 64KiB 버퍼 2개, 5회 중앙값이며 correctness error는 0이었다.

| 비교 항목 | 중앙값 |
|---|---:|
| 세션별 512개 `VirtualAlloc` + `RIORegisterBuffer` | 3,792.3µs |
| 단일 32MiB arena `VirtualAlloc` + `RIORegisterBuffer` | 1,348.3µs |
| 8개 scatter `RIO_BUF` 준비 | 3.211ns/회 |
| 8×128바이트를 연속 1KiB로 복사해 준비 | 28.0525ns/회 |

arena 등록은 초기화 시간이 64.4% 짧았지만 현재 세션별 double-mapped 저장소는 세션 수명과 실패 롤백을 독립적으로 격리하고 wrap 복사를 없앤다. 등록 비용은 시작 시 한 번만 발생하므로 0.2.0은 현재 구조를 유지하며, 대규모 세션 구성에서 시작 시간이 병목으로 확인될 때 worker arena와 정렬된 offset 타입을 도입한다. 송신은 scatter 준비가 연속 복사보다 약 8.7배 짧았으므로 고정 슬롯의 단일 `RIO_BUF`를 직접 게시하는 현재 경로를 유지한다. 원본 결과는 `benchmarks/results/20260827-171244-7d80c68-design`에 있다.

같은 장비에서 adaptive/64, metrics off, 32연결, 128바이트 payload를 affinity off/on 각각 1초×3회 측정했다. 모든 오류 카운터는 0이었다.

| affinity | median packets/s | median P99 µs | median server CPU |
|---|---:|---:|---:|
| off | 46,480 | 1,323 | 5.235% |
| on | 46,789 | 1,256 | 4.610% |

affinity on은 처리량이 0.7% 높고 P99가 5.1% 낮았지만 3회의 짧은 측정 변동 범위 안이다. 기본 동작을 바꿀 만큼 명확한 이점이 아니므로 OS scheduler를 사용하는 off를 유지하고, affinity는 실제 배포 장비의 processor group과 NUMA 구성을 측정한 경우에만 opt-in으로 사용한다. 원본 결과는 `benchmarks/results/20260827-171306-7d80c68`에 있다.
