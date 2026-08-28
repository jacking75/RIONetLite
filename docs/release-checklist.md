# 릴리스 체크리스트

## 0.3.0

- [x] VS2026/v145 x64 Debug와 Release 빌드, 경고 0, 단위 56개와 통합 18개 통과
- [x] clang-format, `git diff --check`, 문서·API 링크 검사 통과
- [x] 실제 RIO 8개 시나리오(echo/split/coalesce/half-close/slow-read/abrupt-close/invalid-length/no-read) data error 0
- [x] `36a03af` 대비 송신 경로 A/B 4개 조합에서 처리량 1.6~2.4배, P99 동등 이상 (`docs/performance-tuning.md`)
- [x] `version.h`, `vcpkg.json`, CHANGELOG 버전 일치
- [ ] `v0.3.0` tag 생성과 `tools\verify-version.ps1 -ExpectedVersion 0.3.0`
- [ ] 고정 장비 benchmark 기준선 재승인 (송신 경로 변경으로 0.2.x 기준선과 직접 비교 불가)
- [ ] 10분 이상 stress soak와 ASan RIO 재실행

`docs/perf-stability-audit-20260828.md`의 안정성 10건, 성능 5건, 기능 2건을 모두 반영했다. 남은 항목은 고정 장비 기준선 재승인과 장시간 soak다.

## 0.2.0

- [x] VS2026/v145 x64 Debug와 Release 빌드, unit/integration 통과
- [x] clang-format dry-run, `git diff --check`, `/analyze` 통과
- [x] 최종 후보 10분 stress 통과 (`artifacts/stress/20260827-171938-7d80c68-final-10m`)
- [x] 1시간 재검증과 24시간 scheduled soak는 사용자 승인으로 최종 게이트에서 제외
- [x] 고정 장비 benchmark의 9개 케이스가 처리량/P99 회귀 기준 통과 (`benchmarks/results/20260827-171359-7d80c68`)
- [x] mimalloc on/off 기능 및 성능 결과 기록
- [x] package consumer가 artifact headers/lib만으로 빌드
- [x] `v0.2.0` tag, `version.h`, `vcpkg.json`, CHANGELOG, package VERSION 일치
- [x] 미해결 P0/P1 없음, package PDB와 source tag 보존

장시간 게이트의 범위 변경은 사용자 승인에 따른 것이다. 중단된 1시간 산출물은 통과 근거로 사용하지 않으며, 위 10분 final soak와 고정 장비 benchmark를 `v0.2.0` 릴리스 근거로 사용한다.

추가 검증 산출물은 ASan RIO 8개 시나리오 `artifacts/memory-safety/20260827-171207-asan-rio`, 설계 벤치마크 `benchmarks/results/20260827-171244-7d80c68-design`, affinity 비교 `benchmarks/results/20260827-171306-7d80c68`에 있다.

GitHub Actions workflow는 사용자 요청으로 릴리스 트리에서 제거했다. 동일 검사는 `tools/`의 로컬 스크립트로 실행한다.

## P0/P1 위험 추적

| 위험 | 해결 코드 | 회귀 근거 |
|---|---|---|
| 세션 과다 생성·잘못된 worker 배정 | `src/detail/session_partition.h`, `RIONetLite/impl/session_manager.h` | `SessionPartitionTest.AssignsEverySessionExactlyOnce` |
| 실행 플래그와 close 경쟁 | `RIONetLite/impl/server_net*.h`, `src/detail/shutdown_coordinator.h` | `ConcurrentStopCallsWaitForTheSameShutdown`, `ImmediateStopRacesWithConcurrentSendsWithoutHang` |
| CQ 참조 수명·완료 배열 해제 | `RIONetLite/impl/server_net_io_worker.h` | Debug/Release 통합 suite와 `/analyze` |
| 지연 종료 큐의 동시 생성 | `RIONetLite/impl/server_net_lazyed_dis_conn_manager.h` | `StartsAndStopsWithoutConnections`, stress의 abrupt-close |
| outstanding 송신 버퍼 덮어쓰기 | `RIONetLite/impl/send_immediately.h`, `RIONetLite/impl/rio_context.h` | `SendQueueTest.*` |
| CQ·등록 버퍼 조기 해제 | `RIONetLite/impl/server_net.h`, `src/detail/shutdown_coordinator.h`, `src/detail/win32_resources.h` | `DoesNotDeregisterOutstandingMemory`, graceful/immediate stop 통합 테스트 |
| 오류 완료·stale generation 처리 | `RIONetLite/impl/rio_context.h`, `RIONetLite/impl/server_net_io_worker.h` | `ProducesDeterministicErrorAndCorruptCompletions`, `ReusesSlotWithNewGenerationAndRejectsStaleId` |
| 수신 소비량·경계 위반 | `RIONetLite/impl/ring_buffer.h`, `include/rionetlite/packet_decoder.h` | `RecvBufferTest.*`, `ClosesWhenCallbackConsumesBeyondReadableBytes`, fragmentation/coalescing·최대 프레임 통합 테스트 |
| accept/RQ 실패 소켓 누수 | `RIONetLite/impl/server_net_accepter.h` | `UniqueSocket` RAII와 N번째 RIO 실패 fake |
| TCP_NODELAY 적용 대상 오류 | `RIONetLite/impl/tcp_session.h` | 실제 RIO connect/echo 통합 suite |
| 초기화·시작 중간 실패 롤백 | `RIONetLite/impl/server_net_init.h`, `src/server.cpp` | `BindFailureRollsBackAndAllowsRetry`, `FailsConfiguredNthCallForEveryFallibleRioOperation` |
| 전역 logger와 null 호출 | `include/rionetlite/event_handler.h`, `src/server.cpp` | `TwoServersUseIndependentResourcesAndLoggers` |
| 링 버퍼 full/empty·해제 오류 | `RIONetLite/impl/ring_buffer.h`, `RIONetLite/impl/send_immediately.h` | `RecvBufferTest.*`, `SendQueueTest.*` |
| 예제 packet 검증·무제한 큐 | `packet_decoder.h`, `queued_event_adapter.h`, ChatServer | decoder/queue 단위 테스트와 invalid-length stress |
| 콜백 예외로 인한 worker 종료 | `src/server.cpp` | `IsolatesHandlerExceptionsAndKeepsServing` |
| 종료 이벤트 유실 | `src/queued_event_adapter.cpp` | `DeliversCloseAfterTheRingDrainsWhenQueueWasFull`, `HoldsBackANewGenerationUntilTheParkedCloseIsSeen` |
| close 이후 수신 콜백 전달 | `RIONetLite/impl/server_net_io_worker.h` | `NeverDeliversDataAfterCloseWhenClosingFromCallback` |
| index 단독 강제 종료 | `RIONetLite/impl/server_net.h`, `RIONetLite/impl/session_manager.h` | `ForcedCloseIgnoresAStaleGeneration` |
| cold-start worker 편중 | `RIONetLite/impl/session_manager.h` | `LeasesSessionsRoundRobinAcrossWorkersFromColdStart` |
| drain 실패 시 자원 조기 회수 | `RIONetLite/impl/server_net.h`, `src/win32_resources.cpp` | 소켓 abort 후 재drain, 실패 시 소유권 포기 |

최종 release commit을 만든 뒤 이 표와 동일한 트리의 tag를 `tools\verify-version.ps1 -ExpectedVersion 0.3.0`으로 확인한다.
