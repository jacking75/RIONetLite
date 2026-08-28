# 설정

| 설정 | 기본값 | 범위/단위 | 영향 |
|---|---:|---|---|
| `port` | 0 | 1..65535 | listen port, 0은 거부 |
| `io_worker_count` | CPU 기반 | 1..min(`max_sessions`, 256) | CQ/스레드 수 |
| `max_sessions` | 1024 | 1..INT32_MAX | 모든 메모리 예산에 선형 영향 |
| `max_receive_chunk_bytes` | 4096 | 1..recv buffer | RIO recv 길이 |
| `receive_buffer_bytes` | 64 KiB | page 배수, ≤1 GiB | 세션별 등록/매핑 |
| `max_message_bytes` | 16 KiB | 양쪽 buffer 이하 | 단일 Send/packet 제한 |
| `send_buffer_bytes` | 256 KiB | page 배수, ≤1 GiB | 유계 send 저장소이자 송신 배압 단위 |
| `completion_batch_size` | 64 | 1..4096 | 결과 배열과 batch |
| `worker_wait_mode` | adaptive | polling/adaptive/notification | 지연과 idle CPU |
| `adaptive_spin_count` | 128 | 0..1,000,000 | adaptive spin 비용 |
| `enable_thread_affinity` | false | bool | opt-in affinity |
| `enable_metrics` | true | bool | false이면 hot-path 카운터를 생략하고 `Metrics()`는 0 snapshot 반환 |
| `enable_callback_timing` | true | bool | false이면 콜백당 QPC 2회를 생략하고 `callback_*`만 0이 된다 |
| `graceful_shutdown_timeout_ms` | 5000 | 1..300000 ms | drain 상한 |
| `peer_close_drain_timeout_ms` | 5000 | 1..60000 ms | peer FIN 후 송신 drain 상한 |
| `idle_timeout_ms` | 0 | 0 또는 ≥1000 ms | 0이면 사용 안 함, 무통신 세션 종료 |
| `keepalive_time_ms` | 0 | 0이면 사용 안 함 | TCP keepalive 시작 시점 |
| `keepalive_interval_ms` | 1000 | keepalive 사용 시 ≥1 | TCP keepalive 재시도 간격 |

`ValidateServerConfig`의 `estimated_*_bytes`가 등록, 예약 virtual, commit, control 메모리를 시작 전에 분리 보고한다. page 정렬을 무시하는 설정은 잘못이다. 검증을 통과한 뒤에도 실제 자원 확보가 실패할 수 있으며, `Server::Create`는 이 경우 예외를 외부로 전파하지 않고 `kResourceAllocationFailed`를 반환한다.

## 송신 배압 단위

세션당 in-flight RIO 송신 요청은 1개로 고정된다. 애플리케이션이 보낸 바이트는 세션 송신 링에 쌓이고, 진행 중인 요청이 완료되면 그때까지 쌓인 전체 구간이 하나의 `RIO_BUF`로 다시 게시된다. 따라서 배압은 요청 슬롯 수가 아니라 `send_buffer_bytes`에 남은 바이트로 판정하며, 링에 들어가지 못한 메시지만 `kWouldBlock`이 된다. 소형 패킷이 많은 게임 트래픽에서 완료 이벤트 수가 패킷 수가 아니라 burst 수에 비례하고, RIO completion queue 용량도 세션당 2개로 줄어든다.

## 죽은 연결 회수

`keepalive_time_ms`를 지정하면 세션 소켓에 `SIO_KEEPALIVE_VALS`를 설정한다. 사라진 피어의 미완료 수신이 오류로 완료되어 기존 오류 경로가 슬롯을 회수한다. `idle_timeout_ms`를 지정하면 각 I/O worker가 약 1초 주기로 자기 파티션의 세션만 훑어 마지막 수신 이후 경과 시간이 상한을 넘은 세션을 `kIdleTimeout`으로 닫는다. 두 설정은 독립이며 함께 사용할 수 있다.
