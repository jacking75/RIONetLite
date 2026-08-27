# 백프레셔

모든 큐는 유계다. `Send`의 `kWouldBlock`은 재시도 가능한 포화를, `kMessageTooLarge`는 계약 위반을, `kStaleSession`과 `kClosing`은 재시도하면 안 되는 수명 종료를 뜻한다. 반환값을 무시하거나 무제한 보조 큐를 두는 것은 잘못이다.

`QueuedEventAdapter`는 `Disconnect` 또는 `RejectNewest`를 제공하며 네트워크 worker를 block하는 정책은 제공하지 않는다. 게임 서버는 중요도에 따라 접속 종료, 패킷 폐기 또는 상위 rate limit을 선택하고 queue depth/high-watermark/rejection을 관찰해야 한다.
