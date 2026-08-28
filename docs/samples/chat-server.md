# ChatServer

ChatServer의 네트워크 callback은 `QueuedEventAdapter`에 connect/data/close만 넣고 즉시 반환한다. shard worker가 자신의 session과 room 상태를 소유하며 login → lobby → room 전이, heartbeat/idle timeout, room broadcast를 처리한다.

packet은 protocol version과 big-endian 길이를 명시적으로 검증한다. malformed 입력과 event/send queue 포화는 close 정책으로 이어진다. 종료는 network accept/drain → adapter drain → logic worker join 순서다. 공유 room을 callback에서 직접 잠그거나 payload span을 저장하는 구현은 잘못이다.

응답과 room broadcast는 8바이트 헤더를 스택에 만들고 payload span을 그대로 넘기는 `Send(session, header, payload)`를 사용한다. 패킷마다 임시 벡터를 할당해 헤더와 본문을 이어붙이는 구현은 broadcast 대상 수만큼 할당을 반복하므로 잘못이다.

```powershell
bin\x64\Debug\ChatServer.exe --port 32453 --logic-workers 4 --max-sessions 1024 --idle-seconds 30
```
