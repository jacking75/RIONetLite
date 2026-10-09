# 첫 Echo 서버의 코드 읽기

[EchoServer 전체 코드](../../samples/echo_server/main.cpp)가 빌드되는 한 파일 예제다. 복사한 부분 코드 대신 이 파일을 열고 [첫 실행](../getting-started.md)을 완료한다. README의 C++ 블록은 즉시 종료하는 package smoke 예제이므로 계속 대기하는 서버와 구별한다.

1. `EchoHandler`는 공개 EventHandler를 구현한다. `LengthPrefixedDecoder`의 maximum은 header를 포함한 전체 메시지 크기다.
2. main은 logger와 handler를 먼저 만들고 Server를 나중에 만든다. 역순 파괴 시 handler/logger가 Server보다 오래 산다. Create는 bind와 버퍼 확보를 수행하며 실패 시 Error를 확인한다.
3. `handler.Attach(*server)`는 callback이 Send/RequestClose를 호출할 대상을 연결한다. Start 전에 붙인다. 이 포인터의 수명은 main의 Server 수명 안이다.
4. OnData는 완성된 프레임마다 Send한다. kQueued는 복사·수락이다. kWouldBlock이면 무한 retry하지 않고 overload close를 요청한다. decoder 오류는 protocol close다. 반환값은 DecodeResult.consumed_bytes라 미완성 바이트가 transport에 남는다.
5. main은 quit·Ctrl+C·duration·stop-file을 기다린다. callback에서 Stop을 부르지 않는다. 종료 신호를 주 스레드가 받아 Stop(kGraceful)의 결과를 확인한다.

```text
handler/logger 생성 → Create → Attach → Start
  → OnData에서 decode와 빠른 Send
  → 주 스레드 종료 신호 → Stop 결과 확인 → Server 파괴 → handler/logger 파괴
```

실험은 `LoadClient --scenario split`, `coalesce`, `half-close` 순서로 수행한다. 각 실행의 JSON에서 오류와 누락이 0인지 확인한다. frame 길이를 잘못 만들면 protocol 종료가 정상 결과다. [프레이밍](../packet-framing.md)은 실제 소비량을, [API 계약](../api/contracts.md)은 실패별 동작을 설명한다.

이 예제는 최소 네트워크 사용을 보여준다. 룸·DB·파일 처리는 callback에 추가하지 않고 [게임 워커](game-worker.md)로 넘긴다.
