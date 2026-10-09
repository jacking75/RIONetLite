# RIONetLite

[한국어](README_kr.md)

RIONetLite is an x64 game-server C++ static library built on Windows Registered I/O (RIO). Its baseline contract is session-generation validation, buffer lifetime that lasts until completion, bounded backpressure, and completion-aware shutdown.

0.3.x is a **preview**: API and ABI may change during 0.x. The transport is TCP/IPv4 server-only on Windows x64, with VS2026/MSVC v145 and Windows SDK. Windows 10 1803+, Windows 11 and Server 2019+ are target systems, not a fully tested OS matrix. IPv6, TLS, UDP and client transport APIs are outside scope. See the [English core guide](docs/getting-started-en.md) or the [Korean documentation index](README_kr.md#문서-안내).

## First Echo

You need Visual Studio 2026, MSVC `v145`, Windows SDK 10, and manifest-mode vcpkg. Run the following from a Developer PowerShell prompt.

```powershell
tools\build.ps1 -Configuration Debug -Rebuild
bin\x64\Debug\EchoServer.exe --port 32452
```

From another terminal, verify the real echo and checksum.

```powershell
bin\x64\Debug\LoadClient.exe --port 32452 --connections 4 --duration 10 --scenario echo --json result.json
```

Require client exit 0, nonzero received packets and zero connection failures/data errors/dropped packets/timeouts in result.json. Stop the server with quit and Enter, or Ctrl+C; require exit 0. Initial tool installation and dependency restoration take additional time.

The minimal server is [EchoServer](samples/echo_server/main.cpp). `tools\package.ps1` builds a fresh captured source snapshot and bundles the headers, Release library/PDB, license, changelog, version, and a minimal consumer sample, then builds and verifies the sample using only the files inside the package it just produced.

The package includes source/compiler/SDK/runtime metadata, SHA256 inventories and a ZIP-only consumer check. See [Binary package consumption](docs/package-consumption.md). Development snapshots can be dirty; formal release packaging uses `tools\package.ps1 -RequireReleaseTag` and requires a clean tree tagged with the source version.

## Lifecycle Smoke Example

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

This example immediately stops and checks the lifecycle. For a continuously listening server, use EchoServer and the [code walkthrough (Korean)](docs/tutorial/first-echo-server.md).

Use the API in the order `Server::Create` → `Start` → `Send`/`RequestClose` → `Stop`. The span passed to a callback becomes invalid once the callback returns, so long-running game logic must be handed off to a separate worker through `QueuedEventAdapter`.

Start/Stop from this server's event or logger callbacks is rejected; signal the control thread. Moving/destroying Server requires quiescent application callers. Send copies before return; kQueued does not acknowledge peer processing. See [Function contracts (Korean)](docs/api/contracts.md).

To reclaim dead connections automatically, set `ServerConfig::keepalive_time_ms` and `idle_timeout_ms`. To send a header and payload as a single message, use `Server::Send(session, header, payload)` instead of an application-side temporary buffer. See [Configuration (Korean)](docs/configuration.md) and [Packet Framing (Korean)](docs/packet-framing.md) for the details.

## Documentation

- [Getting Started](docs/getting-started-en.md)
- [Binary Package Consumption](docs/package-consumption.md)
- [Korean documentation index](README_kr.md#문서-안내): the complete technical guides, API reference, samples, protocol, release checklist, five ADRs and the single-file HTML book.

The detailed guides are currently written in Korean and indexed in README_kr.md. Follow [CHANGELOG](CHANGELOG.md) for release changes, [CONTRIBUTING](CONTRIBUTING.md) to contribute, [SECURITY](SECURITY.md) to report security issues and [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES.md) for dependency notices.

## Quality Commands

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

For a longer soak, increase `-DurationSeconds` on the same stress engine, run it manually, and keep the results in an external artifact store. Do not commit large binaries, PDBs, dumps, build output, or benchmark results to Git.
