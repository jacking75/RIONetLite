# 검증과 릴리스

빌드·기능·안정성·성능·배포 검사는 서로 다른 증거다. 이 문서는 실행 방법과 통과 기준, 마지막 공개 준비 수정의 확인 범위를 함께 설명한다. 생성 결과는 `artifacts/` 또는 별도의 artifact 저장소에 보관하며 Git에 커밋하지 않는다.

## 기본 검사

```powershell
tools\check-format.ps1
tools\check-docs.ps1
tools\build.ps1 -Configuration Debug -Rebuild
tools\build.ps1 -Configuration Release -Rebuild
tools\test-chat.ps1 -Configuration Release
tools\run-stress.ps1 -Configuration Release -DurationSeconds 600
tools\package.ps1
```

두 빌드의 exit 0과 unit/integration suite 결과를 확인한다. Release의 Debug allocation hook skip은 pass 수에 합치지 않는다. Chat은 fixture·room 격리·malformed close·full game drain을 확인한다. stress는 모든 경로를 먼저 예열하고 같은 quiescent 상태의 handle·thread·private bytes를 비교한다. 표본 부족·정합성 오류·비정상 서버 exit·강제 종료는 실패다. 600초 통과가 장기 운영이나 고정 장비 성능 승인을 뜻하지 않는다.

## 메모리 안전성

메모리 검사 결과는 사용한 도구, 실행 파일, 구성과 테스트 시나리오를 함께 기록한다. RIO를 지원하지 않거나 실제로 실행하지 않은 sanitizer 구성을 릴리스 통과로 표기하지 않는다.

### MSVC AddressSanitizer x64

Visual Studio Installer에서 C++ AddressSanitizer 구성 요소를 설치한다. 공식 검사는 제품 라이브러리와 EchoServer를 `/fsanitize=address`로 별도 빌드한 뒤 일반 Winsock/IOCP LoadClient로 실제 RIO 정상·오류 시나리오 8개를 실행한다.

```powershell
tools\build.ps1 -Configuration Release -Rebuild
tools\run-memory-safety.ps1
```

스크립트는 **Debug 구성**의 `EnableASAN=true`를 사용해 일반 빌드와 분리된 `artifacts/memory-safety/` 경로에 빌드한다. echo, split, coalesce, half-close, slow-read, abrupt-close, invalid-length와 no-read가 모두 정합성 오류 없이 끝나고 서버 exit code와 sanitizer 보고가 0이어야 통과다. 비계측 GoogleTest 정적 라이브러리는 STL annotation ABI가 다르므로 ASan 실행 파일에 링크하지 않는다.

### Application Verifier와 PageHeap

관리자 PowerShell에서 테스트 실행 파일에만 설정한다. 현재 Windows의 `appverif.exe /?`로 확인한 명령 형식은 다음과 같다.

```powershell
$unit = (Resolve-Path .\bin\tests\x64\Debug\RIONetLite.UnitTests.exe).Path
$integration = (Resolve-Path .\bin\tests\x64\Debug\RIONetLite.IntegrationTests.exe).Path
appverif.exe -enable Heaps Handles Locks Memory Threadpool Leak -for $unit $integration
& $unit
& $integration
appverif.exe -export log -for $unit -with to=.\artifacts\appverif-unit.xml
appverif.exe -export log -for $integration -with to=.\artifacts\appverif-integration.xml
appverif.exe -disable '*' -for $unit $integration
```

Application Verifier의 full PageHeap만 좁게 적용하려면 다음 구성을 사용한다.

```powershell
appverif.exe -enable Heaps -for $unit $integration -with Heaps.full=true
```

Windows SDK Debugging Tools의 `gflags.exe`를 사용하는 동등한 full PageHeap 절차는 다음과 같다. 반드시 테스트 후 설정을 해제한다.

```powershell
gflags.exe /p /enable RIONetLite.UnitTests.exe /full
gflags.exe /p /enable RIONetLite.IntegrationTests.exe /full
& $unit
& $integration
gflags.exe /p /disable RIONetLite.UnitTests.exe
gflags.exe /p /disable RIONetLite.IntegrationTests.exe
```

Application Verifier와 gflags 설치 여부는 환경마다 다르다. 실제 증거에는 실행한 도구만 기록하고 미설치·미실행 도구는 통과로 표시하지 않는다.

### 통과 판정과 보존 자료

테스트 exit code가 0이고 sanitizer 보고, verifier stop, heap corruption과 handle 오류가 없어야 통과다. 실패 시 seed, 명령행, Debug symbols, Application Verifier XML 또는 sanitizer 로그와 관련 stress JSON/CSV를 같은 artifact 디렉터리에 보존한다.

## 성능 측정

기본값은 logical processor에서 한 코어의 애플리케이션 여유를 남긴 worker 수, batch 64, adaptive 128 spin이다. polling은 최고 부하에서 지연을 낮출 수 있지만 idle 코어를 소비하고, notification은 idle 효율이 좋으며, adaptive는 둘의 절충이다. affinity는 NUMA와 processor group을 측정한 뒤에만 켠다.

한 번에 worker, batch(32/64/128/256), wait mode, buffer, allocator 중 하나만 바꾸고 `tools\run-benchmark.ps1` 결과를 비교한다. correctness error가 0이 아닌 결과를 성능 개선으로 채택하면 안 된다. mimalloc은 최종 executable이 선택적으로 링크하며 RIO 등록 메모리 소유와 무관하다.

Release를 빌드한 뒤 `tools\run-benchmark.ps1 -Profile suite -Runs 5 -DurationSeconds 10 -Connections 32`를 실행한다. `suite`는 latency 32/128 B, throughput 1/4/16 KiB, fan-in, burst, slow consumer와 churn을 실행한다. `matrix`는 `-WaitModes`, `-BatchSizes`, `-ObservabilityModes`, `-AffinityModes`의 조합을 비교한다. 결과는 `artifacts/benchmarks/<date>-<commit>/`에 raw JSON, `summary.csv`, 중앙값·표준편차·서버 CPU·컨텍스트 스위치·자원 최고치를 담은 `aggregate.csv`, 환경 JSON과 요약으로 저장된다.

Latency(32/128 B), throughput(1/4/16 KiB), fan-in, burst, slow consumer, churn을 분리한다. worker wait mode, batch, affinity, mimalloc을 한 번에 하나만 변경한다. P99, packets/bytes, CPU와 resource 추세를 함께 보고 data/connection error가 있는 실행은 무효다. 큰 ETW/WPA trace는 Git에 넣지 않고 artifact 저장소에 보관한다.

LoadClient JSON의 `data_errors`, `dropped_packets`, `timeouts`가 모두 0인 결과만 사용한다. 정상 응답 시나리오는 측정 종료 뒤 최대 5초 동안 진행 중 왕복을 drain하므로 종료 경계의 미수신 패킷도 성공으로 숨기지 않는다.

고정 장비의 승인된 `aggregate.csv`를 기준선으로 보관한 뒤 `-BaselineCsv <path>`를 지정하면 같은 case/wait/batch/observability를 비교한다. 기준 디렉터리에는 `approval.json`과 `environment.json`이 함께 있어야 하며 현재 컴퓨터명, CPU, 논리 코어 수와 측정 조건이 모두 일치해야 실행된다. 저장소에 보관하던 0.2.x 기준선과 로컬 결과 디렉터리는 제거했다. 현재 승인된 기준선은 제공하지 않으며 새로운 기준선은 외부 artifact 저장소에서 별도로 관리한다. 기본 실패 조건은 처리량 중앙값 5% 초과 하락 또는 P99 중앙값 10% 초과 증가이며 `regression.csv`를 남긴다.

하네스는 각 측정 중 `\Thread(EchoServer*)\Context Switches/sec`를 1초마다 표본화한다. `summary.csv`에는 실행별 평균·최댓값·표본 수, `aggregate.csv`에는 중앙값을 기록하므로 관리자 권한이 없는 장비에서도 wait mode 비교가 가능하다. `-CaptureEtw`는 추가로 WPR CPU file-mode trace를 `cpu-context-switches.etl`로 저장하며, 스레드별 스케줄링 원인 분석용이다. WPR 시스템 성능 프로파일 정책이 허용된 관리자 환경이 필요하다. 2026-08-27 로컬 실행에서는 정책 오류 `0xc5585011`로 ETW trace 시작이 거부됐지만 성능 카운터 계측은 정상 동작했다.

`tools\run-design-benchmark.ps1`은 실제 RIO 함수 테이블로 세션별 버퍼 등록과 단일 arena 등록의 초기화 시간을 비교한다. 같은 실행에서 8개 `RIO_BUF` descriptor 준비와 8×128바이트를 1KiB 연속 영역으로 복사하는 준비 비용도 비교한다. 컴파일러가 준비 작업을 제거하지 못하도록 결과 메모리를 volatile 간접 호출에 노출하고, 5회 중앙값과 환경을 `design.json` 및 `environment.json`에 남긴다. 이는 초기화와 CPU 준비 비용의 미시 측정이며 네트워크 처리량 비교는 공식 suite가 담당한다.

`tools\test-mimalloc.ps1`은 allocator off/on의 기능과 Echo 부하를 비교한다. 설계 비교·allocator 비교 결과도 기본적으로 `artifacts/benchmarks/`에 생성한다. 과거 수치 표와 장비 기준선은 현행 코드의 승인 결과로 사용하지 않으며, 필요한 이력은 Git에서 확인한다.

## 릴리스 판정

아래 완료 항목은 2026-10-09 공개 준비 수정 당시 수행한 로컬 검사 이력이다. 문서 통합만으로 다시 실행한 검사나 운영 승인을 뜻하지 않는다. 새 release 후보는 동일한 코드·도구 체인·설정으로 다시 판단한다.

- [x] VS2026/v145 x64 Debug/Release Rebuild 경고 0·오류 0이다.
- [x] Debug unit 63 pass·RIO integration 25 pass다.
- [x] Release unit 62 pass/Debug allocation hook 1 skip·RIO integration 25 pass다.
- [x] ASan Debug x64의 실제 RIO 8개 시나리오·sanitizer 오류 0·서버 exit 0이다.
- [x] .NET 10 C# 공유 fixture와 1/2/4 logic shard Chat E2E가 room 격리·21 broadcast·malformed 5개·full drain을 통과했다.
- [x] 예열·quiescent 비교의 90초 stress에서 handle growth 0·정합성 오류 0·exit 0이다.
- [x] 최종 600초 stress: 표본 77·quiescent 20·handle/thread/private bytes 증가 0·정합성 오류 0·서버 exit 0이다.
- [x] SDK 10.0.26100.0·compiler 19.51.36256.0 기록, fresh Release /MD non-/GL snapshot와 최종 ZIP-only consumer build/실행 exit 0이다.
- [ ] 실제 WinForms 화면·별도 깨끗한 PC·대상 OS 조합 검사다.
- [ ] 실제 신입 독자의 문서만 이용한 실습이다.
- [ ] 고정 장비의 현행 source 성능 기준선 승인이다.
- [ ] 깨끗한 release commit/tag와 RequireReleaseTag 검사 및 공개 승인이다.

결과는 실행별 artifacts에 보관한다. dirty source snapshot은 개발 후보다. 위 로컬 검사를 운영 승인이나 모든 OS 인증으로 소개하지 않는다. 최종 package에는 source tree/file/ZIP SHA256을 기록한다.

최종 release commit의 버전과 tag는 `tools\verify-version.ps1 -ExpectedVersion 0.3.0 -RequireReleaseTag`로 검사한다. 개발 후보 packaging과 정식 release packaging은 구분하며, 정식 패키지는 `tools\package.ps1 -RequireReleaseTag`를 사용한다.

## 회귀 근거

| 위험 | 해결 코드 | 회귀 근거 |
|---|---|---|
| 세션 과다 생성·잘못된 worker 배정 | `src/detail/session_partition.h`, `RIONetLite/impl/session_manager.h` | `SessionPartitionTest.AssignsEverySessionExactlyOnce` |
| 실행 플래그와 close 경쟁 | `RIONetLite/impl/server_net*.h`, `src/detail/shutdown_coordinator.h` | `ConcurrentStopCallsWaitForTheSameShutdown`, `ImmediateStopRacesWithConcurrentSendsWithoutHang` |
| CQ 참조 수명·완료 배열 해제 | `RIONetLite/impl/server_net_io_worker.h` | Debug/Release 통합 suite |
| close 제어의 동시성·중복 | `RIONetLite/impl/server_net_lazyed_dis_conn_manager.h` | worker_control 단위 검사, `StartsAndStopsWithoutConnections`, stress의 abrupt-close |
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
