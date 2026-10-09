# 스레딩 모델

accept 스레드는 OnConnected를 호출하고 I/O 워커는 자신의 세션 OnData/OnClosed를 처리한다. 같은 세션의 OnConnected가 끝난 뒤 다른 callback을 전달하며 수신과 종료 순서를 보존한다. 다른 세션과 Logger는 동시 실행될 수 있어 handler/logger의 공유 상태는 사용자가 동기화한다.

Send·RequestClose·Stop·Metrics는 여러 애플리케이션 스레드에서 호출할 수 있다. Stop은 실행 상태를 닫고 이미 진입한 transport 호출자를 배출한다. 객체 이동·파괴는 application 호출자 종료 후 수행한다. event/logger callback 안에서 같은 서버의 Start/Stop은 kInvalidState다. 종료가 필요하면 atomic flag나 control queue로 주 스레드에 알린다. callback에서 다른 스레드를 만들고 그 스레드의 Stop을 기다려도 join 교착이므로 기다리지 않는다.

callback에서 DB·파일·게임 로직을 수행하거나 span을 저장하지 않는다. QueuedEventAdapter가 owned raw bytes로 넘기고 게임 워커가 처리한다. adapter는 유계 저장소와 짧은 mutex를 사용한다. capacity 확보 실패 시 생산자를 무한 대기시키지는 않지만 mutex 경합 대기는 존재한다.

OnData 예외는 protocol close다. OnConnected/OnClosed와 Logger.Log 예외는 격리·기록 또는 무시한다. Logger.IsEnabled는 noexcept 계약을 지킨다. 커널은 별도의 비중첩 링 구간에 다음 수신을 게시하므로 callback span을 반환 전까지만 읽는다. 함수별 조건은 [API 계약](api/contracts.md)에 있다.
