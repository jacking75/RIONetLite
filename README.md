# RIONetLite

RIONetLite는 Windows Registered I/O(RIO)를 사용하는 x64 게임 서버용 C++ 정적 라이브러리다. 세션 generation 검증, 완료 전 버퍼 수명, 유계 백프레셔와 결정론적 종료를 기본 계약으로 삼는다.

## 5분 시작

필요 환경은 Visual Studio 2026, MSVC `v145`, Windows SDK 10, manifest-mode vcpkg다. Developer PowerShell에서 실행한다.

```powershell
tools\build.ps1 -Configuration Debug -Rebuild
bin\x64\Debug\EchoServer.exe --port 32452
```

다른 터미널에서 실제 echo와 checksum을 검증한다.

```powershell
bin\x64\Debug\LoadClient.exe --port 32452 --connections 4 --duration 10 --scenario echo --json result.json
```

최소 서버는 [EchoServer](samples/echo_server/main.cpp)다. `tools\package.ps1`은 headers, Release library/PDB, 라이선스, changelog, version과 최소 consumer sample을 묶고, 생성 중인 패키지 내부 파일만으로 sample을 빌드해 검증한다.

## 공개 API 흐름

```cpp
#include <rionetlite/server.h>

class Handler final : public rionetlite::EventHandler {
 public:
  void OnConnected(rionetlite::SessionId) override {}
  std::size_t OnData(rionetlite::SessionId,
                     std::span<const std::byte> data) override {
    return data.size();
  }
  void OnClosed(rionetlite::SessionId, rionetlite::CloseReason) override {}
};

int main() {
  Handler handler;
  rionetlite::ServerConfig config;
  config.port = 32452;
  auto server = rionetlite::Server::Create(config, handler);
  if (!server || !server->Start()) {
    return 1;
  }
  return server->Stop() ? 0 : 1;
}
```

`Server::Create` → `Start` → `Send`/`RequestClose` → `Stop` 순서로 사용한다. 콜백의 span은 콜백이 반환되면 무효이며, 오래 걸리는 게임 로직은 `QueuedEventAdapter`로 별도 워커에 전달해야 한다.

## 문서

- [설치와 첫 서버](docs/getting-started.md), [구성](docs/configuration.md), [API 레퍼런스](docs/api/README.md)
- [아키텍처](docs/architecture.md), [스레딩](docs/threading-model.md), [세션 수명](docs/session-lifecycle.md)
- [패킷 프레이밍](docs/packet-framing.md), [백프레셔](docs/backpressure.md), [종료](docs/graceful-shutdown.md)
- [성능 튜닝](docs/performance-tuning.md), [벤치마크](docs/benchmarking.md), [메모리 안전성](docs/memory-safety.md), [관측성](docs/observability.md)
- [EchoServer](docs/samples/echo-server.md), [ChatServer](docs/samples/chat-server.md), [문제 해결](docs/troubleshooting.md)

지원 플랫폼과 설계 결정은 [ADR](docs/adr/0001-supported-platform.md)에서 확인한다. 릴리스 변경은 [CHANGELOG](CHANGELOG.md), 보안 보고는 [SECURITY](SECURITY.md)를 따른다.

## 품질 명령

```powershell
tools\check-format.ps1
tools\build.ps1 -Configuration Debug -Rebuild
tools\build.ps1 -Configuration Release -Rebuild
tools\run-memory-safety.ps1
tools\run-stress.ps1 -Configuration Release -DurationSeconds 600
tools\run-benchmark.ps1
tools\run-design-benchmark.ps1
tools\package.ps1
```

장시간 soak가 필요하면 같은 stress 엔진의 `-DurationSeconds`를 늘려 수동 실행하고 결과를 외부 artifact 저장소에 보관한다. 큰 바이너리, PDB, dump, 빌드 출력과 벤치마크 결과는 Git에 커밋하지 않는다.
