# 벤치마킹

Release를 빌드한 뒤 `tools\run-benchmark.ps1 -Profile suite -Runs 5 -DurationSeconds 10 -Connections 32`를 실행한다. `suite`는 latency 32/128 B, throughput 1/4/16 KiB, fan-in, burst, slow consumer와 churn을 실행한다. `matrix`는 `-WaitModes`, `-BatchSizes`, `-ObservabilityModes`, `-AffinityModes`의 조합을 비교한다. 결과는 `benchmarks/results/<date>-<commit>/`에 raw JSON, `summary.csv`, 중앙값·표준편차·서버 CPU·컨텍스트 스위치·자원 최고치를 담은 `aggregate.csv`, 환경 JSON과 요약으로 저장된다.

Latency(32/128 B), throughput(1/4/16 KiB), fan-in, burst, slow consumer, churn을 분리한다. worker wait mode, batch, affinity, mimalloc을 한 번에 하나만 변경한다. P99, packets/bytes, CPU와 resource 추세를 함께 보고 data/connection error가 있는 실행은 무효다. 큰 ETW/WPA trace는 CI artifact에만 둔다.

LoadClient JSON의 `data_errors`, `dropped_packets`, `timeouts`가 모두 0인 결과만 사용한다. 정상 응답 시나리오는 측정 종료 뒤 최대 5초 동안 진행 중 왕복을 drain하므로 종료 경계의 미수신 패킷도 성공으로 숨기지 않는다.

고정 장비의 승인된 `aggregate.csv`를 기준선으로 보관한 뒤 `-BaselineCsv <path>`를 지정하면 같은 case/wait/batch/observability를 비교한다. 기준 디렉터리에는 `approval.json`과 `environment.json`이 함께 있어야 하며 현재 컴퓨터명, CPU, 논리 코어 수와 측정 조건이 모두 일치해야 실행된다. 0.2.x 기준은 `benchmarks/baselines/heungbae-1-vs2026/`다. 0.3.0의 송신 경로 변경으로 이 기준선과는 직접 비교하지 않는다. 기본 실패 조건은 처리량 중앙값 5% 초과 하락 또는 P99 중앙값 10% 초과 증가이며 `regression.csv`를 남긴다.

하네스는 각 측정 중 `\Thread(EchoServer*)\Context Switches/sec`를 1초마다 표본화한다. `summary.csv`에는 실행별 평균·최댓값·표본 수, `aggregate.csv`에는 중앙값을 기록하므로 관리자 권한이 없는 장비에서도 wait mode 비교가 가능하다. `-CaptureEtw`는 추가로 WPR CPU file-mode trace를 `cpu-context-switches.etl`로 저장하며, 스레드별 스케줄링 원인 분석용이다. WPR 시스템 성능 프로파일 정책이 허용된 관리자 또는 CI 계정이 필요하다. 2026-08-27 로컬 실행에서는 정책 오류 `0xc5585011`로 ETW trace 시작이 거부됐지만 성능 카운터 계측은 정상 동작했다.

`tools\run-design-benchmark.ps1`은 실제 RIO 함수 테이블로 세션별 버퍼 등록과 단일 arena 등록의 초기화 시간을 비교한다. 같은 실행에서 8개 `RIO_BUF` descriptor 준비와 8×128바이트를 1KiB 연속 영역으로 복사하는 준비 비용도 비교한다. 컴파일러가 준비 작업을 제거하지 못하도록 결과 메모리를 volatile 간접 호출에 노출하고, 5회 중앙값과 환경을 `design.json` 및 `environment.json`에 남긴다. 이는 초기화와 CPU 준비 비용의 미시 측정이며 네트워크 처리량 비교는 공식 suite가 담당한다.
