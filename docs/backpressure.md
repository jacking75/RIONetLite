# 백프레셔

모든 큐는 유계다. `Send`의 `kWouldBlock`은 재시도 가능한 포화를, `kMessageTooLarge`는 계약 위반을, `kStaleSession`과 `kClosing`은 재시도하면 안 되는 수명 종료를 뜻한다. 반환값을 무시하거나 무제한 보조 큐를 두는 것은 잘못이다.

송신 배압의 단위는 in-flight 요청 수가 아니라 `send_buffer_bytes`다. 세션당 RIO 송신 요청은 항상 1개이고, 진행 중인 요청이 완료되면 그 사이에 쌓인 바이트 전체를 하나의 요청으로 묶어 다시 게시한다. 따라서 소형 패킷 burst는 완료 이벤트를 늘리지 않고 링에 합쳐지며, 링에 들어가지 못한 메시지만 `kWouldBlock`이 된다. 버스트 크기를 링 크기로 나눠 `send_buffer_bytes`를 정하고, `send_would_block` 카운터로 검증하는 것이 올바르다.

`QueuedEventAdapter`는 `Disconnect` 또는 `RejectNewest`를 제공하며 네트워크 worker를 block하는 정책은 제공하지 않는다. 게임 서버는 중요도에 따라 접속 종료, 패킷 폐기 또는 상위 rate limit을 선택하고 queue depth/high-watermark/rejection을 관찰해야 한다.

종료 이벤트는 폐기 대상이 아니다. 큐가 가득 차 `kClosed`를 넣지 못하면 어댑터는 그 이벤트를 보류했다가 해당 세션의 남은 이벤트를 모두 소비한 뒤 순서를 지켜 전달하고, 그 전까지 같은 슬롯의 새 세대 `kConnected`를 받지 않는다. `kData`만 정책에 따라 거부된다.
