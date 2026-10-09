# Getting started

RIONetLite 0.3.x is a preview static library for Windows x64 TCP/IPv4 servers. 0.x API/ABI stability is not guaranteed. Target systems are Windows 10 1803+, Windows 11 and Windows Server 2019+; this is not a completed OS certification matrix. IPv6, UDP, TLS and client transport APIs are outside scope.

Install Visual Studio 2026, Desktop development with C++, MSVC v145, Windows SDK, Git and manifest-mode vcpkg with MSBuild integration. Initial dependency restoration needs network access. Open Developer PowerShell at the repository root:

```powershell
tools\build.ps1 -Configuration Debug -Rebuild
bin\x64\Debug\EchoServer.exe --port 32452
```

Require build exit 0 and passes in both test suites. Release skips the Debug allocation hook test. When the server prints its listening message, run in a second terminal:

```powershell
bin\x64\Debug\LoadClient.exe --port 32452 --connections 4 --duration 10 --scenario echo --json result.json
```

Require exit 0, four connected clients, nonzero received packets and zero connection failures, data errors, dropped packets and timeouts. Stop the server with `quit` and Enter, or Ctrl+C; require exit 0 without `stop failed`.

Create binds and allocates resources; Start runs workers; Stop drains them. Keep handler/logger alive until after Server destruction. Callback spans are borrowed until return. Copy to owned bounded storage for later game work. OnData returns consumed bytes; incomplete frames return 0 and remain in the transport. TCP can split/combine messages. Send(kQueued) accepts copied bytes, without acknowledging peer processing. Do not retry indefinitely in a callback. Retain both SessionId index and generation. Send/RequestClose/Stop can race safely; movement/destruction requires quiescent application callers. Start/Stop from the same server's event/logger callbacks is rejected; signal a control thread instead.

The README consumer immediately starts and stops. The running EchoServer is `samples/echo_server/main.cpp`. Detailed [API contracts](api/contracts.md), [framing](packet-framing.md) and [Chat protocol](protocol/chat.md) currently have Korean explanations. This English core guide and [binary package guide](package-consumption.md) cover installation and key contracts. Full translated tutorials are not supplied yet.

ChatClient needs .NET 10 SDK: `dotnet build samples/chat_client/csharp_test_client.csproj -c Release`. Start ChatServer on 32453 and two clients in the same room. `tools/test-chat.ps1` checks golden fixtures and real TCP interaction with 1/2/4 logic shards. GUI interaction and clean-PC installation are separate acceptance checks.
