# ChatServer와 C# 클라이언트

게임 룸 처리와 PlayerMap은 별도 게임 워커가 소유한다. 네트워크 callback은 QueuedEventAdapter로 raw stream을 복사한다. 같은 room에 서로 다른 shard 사용자가 있어도 유계 broadcast inbox로 모두 전달한다. [게임 워커 설명](../tutorial/game-worker.md)과 [프로토콜](../protocol/chat.md)을 참고한다.

```powershell
tools\build.ps1 -Configuration Release -Rebuild
bin\x64\Release\ChatServer.exe --port 32453 --logic-workers 2 --io-workers 4 --max-sessions 32
dotnet build samples\chat_client\csharp_test_client.csproj -c Release
samples\chat_client\bin\Release\net10.0-windows\ChatClient.exe
```

클라이언트 빌드에는 .NET 10 SDK, 실행에는 .NET 10 Windows Desktop runtime이 필요하다. 현재 제공하는 WinForms 클라이언트는 8바이트 version 1 protocol을 사용한다. 옛 .NET 5/5바이트 packet 클라이언트와 호환되지 않는다.

1. 서버 listening 출력을 확인한다. 두 번째·세 번째 터미널에서 ChatClient.exe를 각각 실행한다.
2. 화면의 주소 127.0.0.1, 포트 32453, 서로 다른 이름, 방 1을 넣고 **연결·입장**을 누른다. 로그인·방 입장 성공이 표시돼야 한다.
3. 채팅 입력에 hi 또는 한글을 넣고 **채팅 전송**이나 Enter를 누른다. 같은 방의 두 창에 각각 한 번 표시돼야 한다.
4. **연결 종료** 후 한 창을 방 2로 다시 연결한다. 방 1 메시지가 방 2에 표시되면 실패다. 방 이동은 재접속으로 수행한다.
5. heartbeat는 연결 후 5초마다 자동 전송한다. 이름은 UTF-8 1..32바이트, 채팅은 0..256바이트다. 제한 초과는 UI 오류 메시지로 확인한다.
6. 서버에서 quit 또는 Ctrl+C로 종료한다. `Logic drain: active_players=0 queued_events=0 pending_broadcasts=0`과 exit 0을 확인한다.

| CLI | 기본값·의미 |
|---|---|
| --port | 32453이다. |
| --logic-workers | 2, 1..max-sessions다. |
| --io-workers | CPU 기반, 1..min(max-sessions,256)이다. |
| --max-sessions | 1024다. |
| --idle-seconds | 30, 게임 protocol activity 기준이다. |
| --duration | 생략하면 계속 실행한다. 지정할 때 양수 초다. |
| --stop-file | 지정 파일이 생기면 종료한다. |

```powershell
tools\test-chat.ps1 -Configuration Release
```

자동 검사는 같은 C# 송수신 코드를 사용해 1/2/4 logic shard 각각 같은 room 3명·다른 room·UTF-8 분할·2/4/16KiB heartbeat 병합·최대 payload·재접속·malformed close·정상 drain을 확인한다. stdout/JSON은 artifacts/chat에 남긴다. 이 결과는 GUI 버튼·창 배치의 수동 검사와 별도 PC 설치를 대신하지 않는다. 이 서버는 입문 예제이며 인증·영속성·복수 서버 room 분산은 제공하지 않는다.
