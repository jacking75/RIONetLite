# 스레딩 모델

accept thread는 연결을 배정하고 I/O worker는 소유 세션의 완료를 처리한다. `OnData`와 `OnClosed`는 해당 세션의 순서를 보존한다. `Send`, `RequestClose`, `Stop`, `Metrics`는 여러 애플리케이션 스레드에서 호출할 수 있다. handler와 logger 구현의 자체 공유 상태는 사용자가 동기화해야 한다.

콜백에서 대기하거나 동일 서버의 종료를 장시간 막는 것은 잘못이다. `QueuedEventAdapter`를 사용해 `session.index % shard_count`로 전달하고 소비자가 소유 payload를 처리하는 것이 올바르다. 콜백 span을 저장하면 안 되며 반환 전에 복사해야 한다.
