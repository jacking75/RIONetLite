# RIONetLite

RIONetLite is a C++ game-server networking library using Windows Registered I/O.
It prioritizes session identity, asynchronous buffer lifetime and safe shutdown.

0.3.x is a preview: API and ABI may change during 0.x. The transport is TCP/IPv4
server-only on Windows x64, using Visual Studio 2026, MSVC v145, C++ latest and
UTF-8. Windows 10 1803+, Windows 11 and Server 2019+ are target systems; the whole
OS/provider matrix has not been certified. IPv6, TLS, UDP and client transport
APIs are outside scope.

## Start

Install Visual Studio's Desktop development with C++, Windows SDK and vcpkg
components. GoogleTest and optional mimalloc use the root vcpkg manifest.

```powershell
tools\build.ps1 -Configuration Debug -Rebuild
tools\build.ps1 -Configuration Release -Rebuild
bin\x64\Debug\EchoServer.exe --port 32452 --duration 30
```

Open [the offline book (Korean)](docs/interactive_book/index.html) in a browser.
Download the HTML to read and run the interactive models; it needs no server or
internet access. The book has 36 chapters (including five appendices), 20 educational experiments,
33 figures, 12 code walkthroughs and 61 quiz questions, plus embedded source snapshots.
Start with the [first-time reader route](docs/interactive_book/index.html#chapter=reading-guide&section=reading-routes).
The system map, glossary popovers and progress connect scenes to code; archived
material is hidden from the default reading, search and print views. See [first Echo](docs/interactive_book/index.html#chapter=echo-walkthrough),
[API contracts](docs/interactive_book/index.html#chapter=api-reference),
[Chat](docs/interactive_book/index.html#chapter=chat-server) and
[build requirements](docs/interactive_book/index.html#chapter=build-and-run).

## Minimal public API

This lifecycle check matches samples/package_consumer/main.cpp. Use EchoServer
for a continuously listening example. The handler and optional logger must
outlive Server. Signal an owner thread to stop from a callback. Send copies
before returning; kQueued acknowledges admission, not peer processing.

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

## Package and maintenance

```powershell
tools\package.ps1
tools\check-docs.ps1
```

Packaging builds a fresh x64 Release /MD library from the exact source snapshot
and checks a consumer extracted from the final ZIP. Read [ABI and packaging](docs/interactive_book/index.html#chapter=packaging-and-release)
before consuming a binary. A local candidate is distinct from a tagged release;
formal packaging uses a clean version tag and -RequireReleaseTag.

See [validation scope](docs/interactive_book/index.html#chapter=book-maintenance&section=maintenance-acceptance)
and [book generation](docs/interactive_book/index.html#chapter=book-maintenance&section=maintenance-build).
Reading the HTML needs no Node or Python; authoring checks use Python 3.10+,
Node.js 20+, npm and installed Edge. Start with npm.cmd --prefix tools/book ci.

[??? ??](README_kr.md) ? [Contributing](CONTRIBUTING.md) ?
[Security](SECURITY.md) ? [Changelog](CHANGELOG.md) ?
[Third-party notices](THIRD_PARTY_NOTICES.md) ? [License](LICENSE)
