# RIONetLite binary package

This is a 0.3.x preview for Windows x64 TCP/IPv4 servers. The package supplies a fresh Release static library, PDB, public headers and the exact source snapshot used to build it. API and ABI can change during 0.x. IPv6, UDP, TLS, client transport APIs and CMake/vcpkg overlay packages are outside this distribution.

Install Visual Studio 2026 with Desktop development with C++, MSVC v145 and Windows SDK. Open Developer PowerShell, extract the ZIP into a new directory, then run from that directory:

```powershell
msbuild samples\package_consumer\PackageConsumer.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:VcpkgEnabled=false /p:VcpkgEnableManifest=false /p:VcpkgApplocalDeps=false
samples\package_consumer\bin\PackageConsumer.exe
$LASTEXITCODE
```

Exit code 0 means RIO initialization, start and stop succeeded. This consumer immediately stops; it is a lifecycle smoke test. To run an actual listening EchoServer, follow `source/docs/getting-started.md` and build the supplied source. No repository checkout or vcpkg installation is needed for the binary consumer.

For an existing project, select x64 Release, `/std:c++latest`, `/utf-8`, `/MD`, the compiler/SDK versions in `package-manifest.json`, include `include/`, and link `lib/RIONetLite.lib` with `Ws2_32.lib`, `Mswsock.lib` and `Mincore.lib`. Use `<rionetlite/server.h>`. Do not link this Release library into a `/MDd` Debug executable. Build a matching Debug library from `source/` instead. The package disables `/GL`; matching the recorded compiler remains the supported configuration.

`package-manifest.json` records compiler, SDK, runtime, optimization, Git commit, dirty state and source tree SHA256. `source-files.json` maps each source path to its SHA256. `SHA256SUMS.txt` covers delivered files except itself; the sibling `.zip.sha256` covers the final ZIP. A dirty snapshot is a development candidate, not a tagged release. Keep the `.lib`, PDB and source from the same package together.

The packaging script builds from the captured snapshot, extracts the final ZIP, and builds/runs the consumer with global vcpkg integration disabled. `artifacts/package-verification/<version>/verification.json` is the local verification sidecar. A different clean PC and the full target OS matrix require separate acceptance.

If startup fails, record `Error.code`, `native_code`, `field` and `message`. Port 32452 must be free. See `source/docs/operations.md` and `source/SECURITY.md` for diagnosis and private vulnerability reporting. The source includes the Korean tutorial and `source/docs/getting-started-en.md`.
