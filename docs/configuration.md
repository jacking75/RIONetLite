# 설정

| 설정 | 기본값 | 범위/단위 | 영향 |
|---|---:|---|---|
| `port` | 0 | 1..65535 | listen port, 0은 거부 |
| `io_worker_count` | CPU 기반 | 1..min(`max_sessions`, 256) | CQ/스레드 수 |
| `max_sessions` | 1024 | 1..INT32_MAX | 모든 메모리 예산에 선형 영향 |
| `max_receive_chunk_bytes` | 4096 | 1..recv buffer | RIO recv 길이 |
| `receive_buffer_bytes` | 64 KiB | page 배수, ≤1 GiB | 세션별 등록/매핑 |
| `max_message_bytes` | 16 KiB | 양쪽 buffer 이하 | 단일 Send/packet 제한 |
| `send_buffer_bytes` | 256 KiB | page 배수, ≤1 GiB | 유계 send 저장소 |
| `max_outstanding_sends_per_session` | 16 | ≥1, 전체가 buffer에 적합 | CQ/context 수 |
| `completion_batch_size` | 64 | 1..4096 | 결과 배열과 batch |
| `worker_wait_mode` | adaptive | polling/adaptive/notification | 지연과 idle CPU |
| `adaptive_spin_count` | 128 | 0..1,000,000 | adaptive spin 비용 |
| `enable_thread_affinity` | false | bool | opt-in affinity |
| `enable_metrics` | true | bool | false이면 hot-path 카운터를 생략하고 `Metrics()`는 0 snapshot 반환 |
| `graceful_shutdown_timeout_ms` | 5000 | 1..300000 ms | drain 상한 |

`ValidateServerConfig`의 `estimated_*_bytes`가 등록, 예약 virtual, commit, control 메모리를 시작 전에 분리 보고한다. page 정렬을 무시하거나 `max_message * outstanding > send_buffer`인 설정은 잘못이다. 검증을 통과한 뒤에도 실제 자원 확보가 실패할 수 있으며, `Server::Create`는 이 경우 예외를 외부로 전파하지 않고 `kResourceAllocationFailed`를 반환한다.
