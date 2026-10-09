# 배압과 유계 저장소

Send 링은 세션당 send_buffer_bytes다. 공간 부족이면 kWouldBlock이며 수락하지 않은 메시지는 호출자가 소유한다. 게임 워커에서 상한 있는 retry queue, rate limit 또는 kOverloaded close를 선택한다. callback 안의 무한 retry는 완료를 처리할 워커를 막는다.

adapter는 shard별 events_per_shard slot과 slot별 max_payload_bytes를 기동 시 확보한다. raw OnData는 조각으로 나누므로 큰 TCP coalesce를 큰 메시지로 오판하지 않는다. 조각 자체는 메시지가 아니므로 게임 워커에서 framing한다.

| 정책 | 포화 동작 |
|---|---|
| kDisconnect | 거절된 chunk가 생기면 CloseRequest를 호출하고 입력은 소비한다. framed TCP에 사용한다. |
| kRejectNewest | 거절된 raw chunk를 소비·폐기하는 손실 정책이다. 뒤 바이트의 framing을 깨뜨릴 수 있어 framed TCP에 사용하지 않는다. |

close는 폐기하지 않는다. 링 포화 시 세션별 사전 확보 gate에 parked close를 저장하고 선행 이벤트를 소비한 뒤 전달한다. 해당 index의 새 generation connected는 이전 close가 전달될 때까지 거절한다. event_queue_depth는 링과 parked close를 모두 포함하므로 events capacity보다 클 수 있다. 이 보호가 애플리케이션 Player 상태를 자동 정리한다는 뜻은 아니다.

transport close 제어는 세션당 mailbox 하나와 worker당 최대 64개 batch다. 중복 close를 합치고 아직 outstanding인 세션은 다시 보류한다. Chat broadcast inbox도 shard당 1024개로 유계이며 모든 shard에 넣을 공간이 없으면 발신자를 overload close한다.

adapter.metrics의 depth/high_watermark/rejections와 Server.Metrics의 send_would_block을 따로 관찰한다. EstimatedMemoryBytes는 slot payload·gate·parked index 등의 추정이며 allocator·mutex·thread stack·kernel 비용은 process 계측으로 확인한다.
