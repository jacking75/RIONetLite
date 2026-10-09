# 메모리 안전성 검사

메모리 검사 결과는 사용한 도구, 실행 파일, 구성과 테스트 시나리오를 함께 기록한다. RIO를 지원하지 않거나 실제로 실행하지 않은 sanitizer 구성을 릴리스 통과로 표기하지 않는다.

## MSVC AddressSanitizer x64

Visual Studio Installer에서 C++ AddressSanitizer 구성 요소를 설치한다. 공식 검사는 제품 라이브러리와 EchoServer를 `/fsanitize=address`로 별도 빌드한 뒤 일반 Winsock/IOCP LoadClient로 실제 RIO 정상·오류 시나리오 8개를 실행한다.

```powershell
tools\build.ps1 -Configuration Release -Rebuild
tools\run-memory-safety.ps1
```

스크립트는 **Debug 구성**의 `EnableASAN=true`를 사용해 일반 빌드와 분리된 `artifacts/memory-safety/` 경로에 빌드한다. echo, split, coalesce, half-close, slow-read, abrupt-close, invalid-length와 no-read가 모두 정합성 오류 없이 끝나고 서버 exit code와 sanitizer 보고가 0이어야 통과다. 비계측 GoogleTest 정적 라이브러리는 STL annotation ABI가 다르므로 ASan 실행 파일에 링크하지 않는다.

## Application Verifier와 PageHeap

관리자 PowerShell에서 테스트 실행 파일에만 설정한다. 현재 Windows의 `appverif.exe /?`로 확인한 명령 형식은 다음과 같다.

```powershell
$unit = (Resolve-Path .\bin\tests\x64\Debug\RIONetLite.UnitTests.exe).Path
$integration = (Resolve-Path .\bin\tests\x64\Debug\RIONetLite.IntegrationTests.exe).Path
appverif.exe -enable Heaps Handles Locks Memory Threadpool Leak -for $unit $integration
& $unit
& $integration
appverif.exe -export log -for $unit -with to=.\artifacts\appverif-unit.xml
appverif.exe -export log -for $integration -with to=.\artifacts\appverif-integration.xml
appverif.exe -disable '*' -for $unit $integration
```

Application Verifier의 full PageHeap만 좁게 적용하려면 다음 구성을 사용한다.

```powershell
appverif.exe -enable Heaps -for $unit $integration -with Heaps.full=true
```

Windows SDK Debugging Tools의 `gflags.exe`를 사용하는 동등한 full PageHeap 절차는 다음과 같다. 반드시 테스트 후 설정을 해제한다.

```powershell
gflags.exe /p /enable RIONetLite.UnitTests.exe /full
gflags.exe /p /enable RIONetLite.IntegrationTests.exe /full
& $unit
& $integration
gflags.exe /p /disable RIONetLite.UnitTests.exe
gflags.exe /p /disable RIONetLite.IntegrationTests.exe
```

Application Verifier와 gflags 설치 여부는 환경마다 다르다. 실제 증거에는 실행한 도구만 기록하고 미설치·미실행 도구는 통과로 표시하지 않는다.

## 통과 판정과 보존 자료

테스트 exit code가 0이고 sanitizer 보고, verifier stop, heap corruption과 handle 오류가 없어야 통과다. 실패 시 seed, 명령행, Debug symbols, Application Verifier XML 또는 sanitizer 로그와 관련 stress JSON/CSV를 같은 artifact 디렉터리에 보존한다.
