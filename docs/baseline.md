# RIONetLite 기준선

## 기준 정보

- 기준 커밋: `b92c2bdb58a199680d8a8664d3d863220aaf0b9d`
- 측정일: 2026-08-26
- 운영체제: Windows NT 10.0.26200.0 x64
- CPU: Intel64 Family 6 Model 183 Stepping 1, 16 logical processors
- Visual Studio: Visual Studio 2026 Community 18.7
- MSBuild: 18.7.8.30822
- MSVC: 19.44.35228
- 대상 도구 집합: v145
- C++ 표준: `/std:c++latest`

이 문서의 수치는 성능 목표가 아니라 T00 시점 동작을 비교하기 위한 기준이다. 성능 수치는 T18의 고정 부하 도구와 측정 절차가 준비된 뒤 추가한다.

## 기준선 빌드

Developer PowerShell 또는 Visual Studio 개발 환경에서 다음 명령을 순차 실행한다.

```powershell
$msbuild = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe'
& $msbuild samples\echo_server\EchoServer.sln /m /t:Rebuild /p:Configuration=Debug /p:Platform=x64
& $msbuild samples\echo_server\EchoServer.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
& $msbuild samples\chat_server\ChatServer.sln /m /t:Rebuild /p:Configuration=Debug /p:Platform=x64
& $msbuild samples\chat_server\ChatServer.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

현재 Codex 실행 환경은 process environment에 `Path`와 `PATH`가 동시에 존재한다. MSBuild 18은 child process 환경을 만들 때 이를 중복 키로 판단한다. 이 환경에서만 다음 정규화를 빌드 프로세스 앞에 적용한다.

```powershell
$taskPath = $env:Path
Remove-Item Env:Path -ErrorAction SilentlyContinue
Remove-Item Env:PATH -ErrorAction SilentlyContinue
$env:Path = $taskPath
```

## 확인된 기준선 결과

| 대상 | 순차 빌드 | 비고 |
|---|---|---|
| Echo Debug x64 | 성공 | 기존 프로젝트 기준 |
| Echo Release x64 | 성공 | 기존 프로젝트 기준 |
| Chat Debug x64 | 성공 | mimalloc manifest restore 사용 |
| Chat Release x64 | 성공 | mimalloc DLL 배치 확인 |

기존 예제는 Debug와 Release가 모두 `samples/bin/<name>.exe`를 사용한다. 같은 예제를 Debug/Release로 동시에 rebuild하면 `LNK1104`가 발생할 수 있다. T02에서 `bin/<Platform>/<Configuration>`으로 분리한다.

## 알려진 결함과 재현 대상

| ID | 현상 | 재현/검증 위치 | 해결 태스크 |
|---|---|---|---|
| KI-001 | 작업자 수보다 한 분량 많은 세션을 생성한다. | `SessionPartitionTest` | T04 |
| KI-002 | 0 작업자 설정에서 나눗셈 오류가 가능하다. | `ConfigValidationTest` | T04 |
| KI-003 | 작업자 람다가 지역 CQ 변수를 참조 캡처한다. | 정적 검토 및 이후 통합 테스트 | T05/T07 |
| KI-004 | accept, worker와 send 실행 플래그에 data race가 있다. | 이후 stress test | T06/T08 |
| KI-005 | RIO 완료 배열의 초기화/해제 크기가 잘못되었다. | 이후 fake RIO test | T07 |
| KI-006 | 송신 완료 전에 등록 버퍼 영역을 덮을 수 있다. | `RegisteredBufferTest`, 이후 `SendQueueTest` | T05/T10 |
| KI-007 | 수신 callback의 소비 크기를 검증하지 않는다. | 이후 `RecvBufferTest` | T09 |
| KI-008 | RQ 생성 실패 시 accepted socket 누수가 가능하다. | `UniqueSocketTest`, 이후 fault injection | T05 |
| KI-009 | CQ close와 buffer deregister가 outstanding 완료보다 빠를 수 있다. | 이후 shutdown integration test | T08 |
| KI-010 | 전역 log function pointer가 null일 수 있다. | API compile/smoke test | T03/T14 |

T00에서는 위 결함을 숨기지 않고 테스트 이름과 후속 태스크에 연결한다. T04와 T05 범위에 해당하는 설정, 세션 분배와 RAII 결함은 이번 변경에서 회귀 테스트로 전환한다.

## 기준 실행 인자

```powershell
bin\x64\Debug\EchoServer.exe --PortNumber=32452 --IoWorkThreadCount=4 --IoSendThreadCount=1 --MaxConnectionCount=256 --MaxRecvPacketSize=1024 --MaxTry1RecvBufferSize=4096 --MAXRecvCountPer1Sec=5 --MaxRecvBufferSize=65536 --MaxSendPacketSize=1024 --MAXSendCountPer1Sec=5 --MaxSendBufferSize=65536
```

서버를 시작하고 TCP client가 length-prefixed packet을 보낸 뒤 동일 byte가 반환되는지 확인한다. 자동 루프백 실행은 T16에서 추가한다.
