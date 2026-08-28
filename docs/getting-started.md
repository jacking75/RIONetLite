# 설치와 첫 서버

Visual Studio 2026의 Desktop development with C++, MSVC v145, Windows SDK를 설치한다. 루트 `vcpkg.json`이 GoogleTest와 선택형 mimalloc을 복원한다.

```powershell
tools\build.ps1 -Configuration Debug -Rebuild
bin\x64\Debug\EchoServer.exe --port 32452
```

`ServerConfig`를 검증한 뒤 `Server::Create`, `Start`, `Stop`을 호출한다. 실패한 `expected`를 무시하거나 `EventHandler`보다 Server를 오래 살게 하는 사용은 잘못이다. handler와 logger가 Server보다 오래 살도록 두는 것이 올바르다.

LoadClient 정상 검증은 `bin\x64\Debug\LoadClient.exe --port 32452 --scenario echo --duration 10`이다.

## 배포 형식 결정

0.x는 Windows x64/MSVC v145 ABI에 고정된 versioned binary package를 기준으로 한다. vcpkg overlay port와 CMake package는 제공하지 않는다. `tools\package.ps1`이 공개 headers, Release `.lib`/PDB와 `samples/package_consumer`를 묶고, sample의 `RIONetLitePackageRoot` 기본값이 package 루트의 include/lib를 자동 참조한다. ABI가 안정되지 않은 0.x에서 소스 port를 배포해 toolchain 차이를 숨기는 것보다 지원 compiler와 binary를 명시하는 정책을 선택했다.
