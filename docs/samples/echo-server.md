# EchoServer

[첫 실행](../getting-started.md)과 [전체 코드 해설](../tutorial/first-echo-server.md)을 따른다. wire는 header 포함 전체 길이의 big-endian u32다. 완성 frame을 그대로 돌려준다. 예제 callback은 decode와 빠른 send만 수행한다.

| CLI | 기본값·의미 |
|---|---|
| --port | 32452다. |
| --duration | 0, 무기한이다. 양수면 지정 초 뒤 종료한다. |
| --workers | CPU 기반 I/O 워커 수다. |
| --max-sessions | 1024다. 워커 수 이상이어야 한다. |
| --batch | 64다. |
| --spin | 128이다. |
| --wait-mode | adaptive, polling/notification도 가능하다. |
| --affinity | off다. |
| --observability | on, off는 Logger와 Server metrics를 끈다. |
| --stop-file | 해당 파일이 생기면 종료한다. |

LoadClient echo/split/coalesce의 JSON에서 연결 실패·data error·dropped packet·timeout 모두 0을 확인한다. quit 또는 Ctrl+C는 주 스레드에서 graceful Stop을 수행한다. 정상 종료는 exit 0이다. Send 배압은 overload close로 처리한다.
