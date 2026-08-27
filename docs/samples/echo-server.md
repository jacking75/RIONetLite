# EchoServer

EchoServer는 공개 `Server`, `EventHandler`, `LengthPrefixedDecoder`만 사용한다. callback에서 완전한 big-endian length-prefix packet을 즉시 echo하고 포화면 연결을 닫는다. `quit`, Ctrl+C와 `--duration`은 모두 graceful `Stop`으로 모인다.

```powershell
bin\x64\Debug\EchoServer.exe --port 32452 --duration 30
bin\x64\Debug\LoadClient.exe --port 32452 --scenario split --duration 10
```

내부 `impl/` 헤더를 include하거나 한 recv를 한 packet으로 보는 코드는 이 예제의 계약이 아니다.
