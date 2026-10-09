# 관측성

Logger.Log(LogEvent)는 timestamp(ns), level, component, worker, SessionId, operation, library/native error를 전달한다. callback/accept/lifecycle 등에서 동시 호출할 수 있다. string_view는 Log 동안만 유효하므로 비동기 logger queue에 저장하려면 복사한다. Log 예외는 격리한다. IsEnabled는 noexcept이고 짧게 반환한다. 파일 I/O는 별도 logging worker로 넘긴다.

| 지표 | 의미 |
|---|---|
| accepted/closed_connections | 누적 연결·종료 수다. |
| active_connections | 현재 활성 연결 gauge다. |
| received_bytes | 수신 완료로 들어온 누적 바이트다. |
| queued_send_bytes | kQueued로 수락한 누적 바이트다. 대기 queue gauge가 아니다. |
| sent_bytes | 성공한 RIO send completion의 누적 바이트다. 상대 ACK가 아니다. |
| send_operations | 수락한 Send 호출 수다. RIO 게시·완료 수와 다르다. |
| send_would_block/stale_operations/protocol_errors | 배압·오래된 ID·protocol 실패 누적이다. |
| callback_* | count·total/max ns다. total/count로 평균을 구한다. |
| adapter event_queue_depth | 링과 parked close를 포함한 현재 gauge다. |
| adapter high_watermark/rejections/dequeued/wait_* | 최고 저장량·거절·소비 수와 total/max 대기 ns다. |

Server.Metrics와 adapter.metrics는 별도 snapshot이다. Server의 event_queue 필드는 0이고 adapter가 제공한다. 여러 atomic을 합친 eventually consistent 조회이므로 순간 항등식으로 오류를 판정하지 않는다.

enable_metrics=false는 Server 지표를 0으로 반환한다. enable_callback_timing=false는 callback_*만 생략한다. adapter 계측은 별도로 유지된다. Echo --observability off는 Server metrics와 Logger를 끈다.

오류 조사 시 Error.code/native_code/field/message와 LogEvent.operation을 같이 남긴다. 예를 들어 bind_failed와 native 10048은 사용 중인 포트다. native 0은 플랫폼 원인이 제공되지 않은 오류일 수 있다. [문제 해결](troubleshooting.md)을 따른다. 과거 짧은 성능 비교는 현재 변경의 회귀 승인 근거가 아니며 [성능 안내](performance-tuning.md)의 날짜·구성을 확인한다.
