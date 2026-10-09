# 설치와 첫 서버

RIONetLite 0.3.x는 Windows x64 TCP/IPv4 서버용 preview다. 0.x API와 ABI는 변경될 수 있다. 대상 OS는 Windows 10 1803 이상·Windows 11·Windows Server 2019 이상이며 전체 OS 조합에서 검증됐다는 뜻은 아니다. IPv6·TLS·UDP·클라이언트 transport API는 제공하지 않는다.

1. Visual Studio Installer에서 Visual Studio 2026의 **Desktop development with C++**, MSVC v145, Windows SDK를 설치한다. ASan에는 C++ AddressSanitizer 구성 요소도 필요하다.
2. Git과 vcpkg를 준비하고 vcpkg의 MSBuild 통합을 설정한다. 루트 `vcpkg.json`의 manifest 복원은 GoogleTest와 mimalloc을 받는다. 최초 복원에는 네트워크와 추가 시간이 필요하다.
3. Developer PowerShell에서 저장소 루트로 이동한다.

```powershell
git clone https://github.com/jacking75/RIONetLite.git
cd RIONetLite
tools\build.ps1 -Configuration Debug -Rebuild
```

빌드 종료 코드 0과 두 테스트 suite의 `[ PASSED ]`를 확인한다. Release에서는 Debug allocation hook 검사 한 개가 skip될 수 있다. skip을 pass 개수에 합치지 않는다. `RIONetLite.sln`의 `Debug|x64`로도 빌드할 수 있지만 테스트 실행은 위 스크립트로 확인한다.

첫 번째 터미널에서 실행한다.

```powershell
bin\x64\Debug\EchoServer.exe --port 32452
```

`EchoServer listening on port 32452. Type quit or press Ctrl+C to stop.`이 나오면 두 번째 터미널도 루트에서 실행한다.

```powershell
bin\x64\Debug\LoadClient.exe --port 32452 --connections 4 --duration 10 --scenario echo --json result.json
$LASTEXITCODE
Get-Content result.json
```

종료 코드 0, `connected=4`, `received_packets>0`, `connection_failures/data_errors/dropped_packets/timeouts=0`이면 실제 왕복에 성공한 것이다. 숫자는 실행마다 다르다. 서버에서 `quit`과 Enter 또는 Ctrl+C로 끝낸다. `stop failed` 없이 종료 코드가 0이어야 한다. 최초 설치·복원까지 5분 안에 끝난다는 보장은 없다.

포트 충돌은 `Get-NetTCPConnection -LocalPort 32452 -State Listen`으로 확인하고 두 프로그램을 같은 다른 포트로 실행한다. [문제 해결](troubleshooting.md)을 참고한다.

다음은 [네트워크 입문](networking-basics.md), [Echo 코드](tutorial/first-echo-server.md), [게임 워커](tutorial/game-worker.md), [Chat 실습](samples/chat-server.md) 순서다. ZIP 소비는 [패키지 안내](package-consumption.md)를 따른다.
