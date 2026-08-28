# 관측성

`Logger::Log(LogEvent)`는 timestamp, severity, component, worker, `SessionId`, operation, library/native error를 연결한다. 비활성 로그는 `IsEnabled`로 hot-path 문자열 생성을 피한다. 라이브러리는 프로세스 전역 exception filter를 바꾸지 않는다.

`Metrics()`는 연결, recv/send bytes와 operation, would-block, stale, protocol error, close reason, CQ corrupt, callback 시간과 adapter queue 지표를 snapshot한다. 평균 callback 대기는 total/count로 계산하며 max와 high-watermark를 함께 확인한다. 서로 다른 Server의 logger와 counter는 공유되지 않는다.

`ServerConfig::enable_metrics=false`이면 hot-path atomic counter와 callback 시간 측정을 생략하고 `Metrics()`는 0 snapshot을 반환한다. counter는 유지하면서 콜백 시간 측정만 끄려면 `enable_callback_timing=false`를 쓴다. 이 경우 콜백당 `QueryPerformanceCounter` 2회가 사라지고 `callback_count`/`callback_total_nanoseconds`/`callback_max_nanoseconds`만 0이 된다. EchoServer의 `--observability off`는 metrics와 logger를 함께 끈다.

`send_operations`는 애플리케이션의 `Send` 호출 수이고 실제 RIO 송신 요청 수는 아니다. 송신이 링에서 합쳐지므로 완료 이벤트는 이보다 적을 수 있다. 배압을 볼 때는 `send_would_block`을 본다.

2026-08-27 Release, adaptive/64, 32연결, 128-byte payload, 2초×5회 비교에서 모든 오류·누락은 0이었다. 중앙값은 off 37,205 packets/s·P99 1,804µs·서버 CPU 4.688%, on 40,668 packets/s·P99 1,495µs·서버 CPU 4.882%였다. 짧은 측정의 표준편차가 각각 3,112/4,904 packets/s여서 유의한 성능 악화는 관찰되지 않았으며, 수치는 고정 장비 장시간 결과를 대신하지 않는다.
