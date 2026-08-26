# AGENTS.md

## 프로젝트 목적

RIONetLite는 Windows Registered I/O(RIO)를 이용하는 게임 서버용 C++ 네트워크 라이브러리다. 변경 시 처리량뿐 아니라 세션 정합성, 비동기 I/O 버퍼 수명, 종료 순서와 장애 상황의 안전성을 우선한다.

저장소 루트의 이 지침은 모든 하위 디렉터리에 적용한다.

## 의사소통

- 사용자에게는 한국어로 간결하고 명확하게 보고한다.
- 문장은 존대말보다 `~다` 형태를 사용한다.
- 변경 결과, 검증 명령과 남은 위험을 구체적으로 설명한다.

## 프로젝트 구조

- `RIONetLite/`: 공개 헤더와 라이브러리 구현이다.
- `RIONetLite/impl/`: RIO, 세션, 송수신, 버퍼의 내부 구현이다.
- `samples/echo_server/`: 최소 EchoServer 예제다.
- `samples/chat_server/`: 게임 패킷과 룸 처리를 포함한 ChatServer 예제다.
- `samples/chat_client/`: ChatServer용 C# 테스트 클라이언트다.
- `samples/thirdparty/`: 외부 코드다. 명시적인 필요가 없으면 수정하거나 포맷하지 않는다.

## 빌드 기준

- Visual Studio 2026과 MSVC `v145`를 기준으로 한다.
- 모든 C++ 구성은 `/std:c++latest`와 `/utf-8`을 유지한다.
- 지원 플랫폼은 x64뿐이다. x86/Win32 구성을 다시 추가하지 않는다.
- x64 Debug/Release 프로젝트 구성을 모두 유지한다.
- ChatServer는 manifest-mode vcpkg로 mimalloc을 사용한다. allocator 바이너리를 저장소에 직접 추가하지 않는다.

대표 검증 명령은 다음과 같다.

```powershell
msbuild samples\echo_server\EchoServer.sln /m /t:Rebuild /p:Configuration=Debug /p:Platform=x64
msbuild samples\echo_server\EchoServer.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
msbuild samples\chat_server\ChatServer.sln /m /t:Rebuild /p:Configuration=Debug /p:Platform=x64
msbuild samples\chat_server\ChatServer.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

플랫폼 또는 공용 헤더를 변경했다면 두 예제의 x64 Debug/Release를 모두 빌드한다.

## C++ 스타일

- Google C++ Style Guide와 루트 `.clang-format`을 따른다.
- 새 파일명은 소문자 `snake_case`를 사용한다.
- 타입은 `PascalCase`, 함수는 `PascalCase`, 지역 변수와 매개변수는 `snake_case`, 상수는 `kCamelCase`를 사용한다.
- 고정 폭 네트워크·프로토콜 값은 `<cstdint>` 타입을 사용한다.
- `NULL`보다 `nullptr`, C 스타일 캐스트보다 C++ 캐스트, 수동 자원 해제보다 RAII를 우선한다.
- 공개 API를 변경할 때는 예제와 README를 함께 갱신한다.
- `c2_rio_net`, `c2rionet`, `RIONetLib`과 제거된 allocator 이름을 다시 도입하지 않는다.
- Windows 헤더는 `WinSock2.h`가 `MSWSock.h`보다 먼저 포함되어야 한다.

C++ 변경 후 다음 검사를 수행한다.

```powershell
clang-format --dry-run --Werror --style=file <변경한 C++ 파일 목록>
git diff --check
```

## 네트워크 구현 불변 조건

- 네트워크 콜백에서 게임 로직, DB, 파일 I/O처럼 오래 걸리는 작업을 직접 수행하지 않는다.
- 애플리케이션 처리는 별도 게임 로직 큐와 워커로 전달한다.
- 세션 식별은 `session_index`와 `session_uid`의 조합으로 검증한다. 인덱스만으로 지연된 송신 완료나 연결 종료를 처리하지 않는다.
- RIO에 등록한 메모리와 `RIO_BUF`는 요청이 완료되기 전에 이동하거나 해제하지 않는다.
- 동일 세션의 수신 순서와 연결 종료 순서를 보존한다.
- 부분 송신, 원격 종료, 큐 포화, 서버 강제 종료 경로를 정상 경로와 동일하게 검증한다.
- 종료 시 accept, I/O 워커, send 작업, completion queue와 등록 버퍼의 의존 관계를 확인한다.
- 패킷 길이, 버퍼 오프셋과 세션 설정값은 사용 전에 범위를 검사한다.

## 예제 코드 원칙

- EchoServer는 공개 API의 최소 사용 예제로 유지한다.
- ChatServer는 네트워크 이벤트를 게임 로직 스레드로 넘기는 구조를 보여줘야 한다.
- 예제는 라이브러리 내부 구현에 직접 의존하지 않고 가능한 한 공개 헤더를 사용한다.
- 명령행 설정이나 패킷 형식을 바꾸면 관련 C# 클라이언트와 README도 확인한다.

## 변경 및 Git 원칙

- 빌드 결과, IDE 캐시와 사용자별 설정을 커밋하지 않는다.
- C++ 의존성은 가능한 한 vcpkg manifest로 관리한다. 서드파티 바이너리를 직접 추가하지 않는다.
- 사용자 소유의 관련 없는 변경을 수정하거나 커밋하지 않는다.
- 커밋 전 구형 프로젝트명 참조, 포맷, 빌드 결과와 `git status`를 확인한다.
