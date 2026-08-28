# 스레딩 모델

accept thread는 연결을 배정하고 I/O worker는 소유 세션의 완료를 처리한다. `OnData`와 `OnClosed`는 해당 세션의 순서를 보존한다. `Send`, `RequestClose`, `Stop`, `Metrics`는 여러 애플리케이션 스레드에서 호출할 수 있다. handler와 logger 구현의 자체 공유 상태는 사용자가 동기화해야 한다.

콜백에서 대기하거나 동일 서버의 종료를 장시간 막는 것은 잘못이다. `QueuedEventAdapter`를 사용해 `session.index % shard_count`로 전달하고 소비자가 소유 payload를 처리하는 것이 올바르다. 콜백 span을 저장하면 안 되며 반환 전에 복사해야 한다.

콜백에서 벗어난 예외는 worker를 죽이지 않는다. `OnData`에서 예외가 나오면 해당 세션을 `kProtocolError`로 닫고 소비 바이트를 0으로 처리하며, `OnConnected`와 `OnClosed`의 예외는 오류 로그만 남기고 무시한다. 다만 예외에 의존해 제어 흐름을 만드는 것은 잘못이며, handler는 자체적으로 예외를 처리해야 한다.

worker는 완료를 처리하기 전에 다음 수신을 먼저 게시한다. 커널이 쓰는 구간과 콜백이 읽는 구간은 세션 수신 링에서 겹치지 않으므로, 콜백이 도는 동안에도 같은 세션의 수신이 진행된다.
