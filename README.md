# RIONetLite

RIONetLite는 Windows Registered I/O(RIO)를 사용하는 게임 서버용 C++ 네트워크 라이브러리다. 네트워크 I/O와 게임 로직을 분리하고, 다수의 TCP 세션을 여러 I/O 워커에서 처리하는 서버를 만드는 것이 목표다.

라이브러리는 헤더 기반으로 제공하며 EchoServer와 ChatServer 예제를 통해 연결, 수신, 송신, 연결 종료와 패킷 처리 흐름을 보여준다.

## 주요 기능

- Windows RIO completion queue/request queue 기반 비동기 TCP I/O
- 복수 I/O 워커 스레드와 세션 풀 관리
- `OnConnect`, `OnReceive`, `OnClose` 네트워크 이벤트 인터페이스
- 세션 인덱스와 UID를 함께 사용하는 재접속·지연 완료 검증
- 즉시 송신과 강제 연결 종료 API
- 수신·송신 링 버퍼 및 RIO 등록 메모리 관리

## 디렉터리 구조

```text
RIONetLite/                 공개 API와 내부 구현
  server_net.h             ServerNetManager 공개 진입점
  i_net_event.h            네트워크 이벤트 인터페이스
  define.h                 설정, 오류 코드, 로그 타입
  impl/                    RIO, 세션, 송수신 내부 구현
samples/
  echo_server/             받은 패킷을 그대로 돌려주는 최소 서버 예제
  chat_server/             로그인, 방 입장, 채팅을 처리하는 게임 서버 예제
  chat_client/             ChatServer 확인용 C# 클라이언트
  thirdparty/               예제용 서드파티 코드
```

## 개발 및 빌드 환경

- Windows
- Visual Studio 2026
- MSVC 플랫폼 도구 집합 `v145`
- 최신 C++ 표준 `/std:c++latest`
- Windows SDK `10.0`

Visual Studio에서 다음 솔루션을 열어 빌드할 수 있다.

- `samples/echo_server/EchoServer.sln`
- `samples/chat_server/ChatServer.sln`

Developer PowerShell에서는 다음과 같이 x64 Debug 구성을 빌드한다.

```powershell
msbuild samples\echo_server\EchoServer.sln /m /t:Build /p:Configuration=Debug /p:Platform=x64
msbuild samples\chat_server\ChatServer.sln /m /t:Build /p:Configuration=Debug /p:Platform=x64
```

두 예제는 Debug/Release와 Win32/x64 구성을 제공한다. ChatServer에 포함된 기존 tcmalloc 바이너리는 x64 Debug에서만 사용하며, 나머지 구성은 MSVC 기본 할당자를 사용한다.

## 기본 사용 흐름

1. `rionetlite::INetEventManager`를 상속해 네트워크 이벤트를 구현한다.
2. `rionetlite::NetConfig`에 포트, 워커 수, 세션 수와 버퍼 제한을 설정한다.
3. `rionetlite::ServerNetManager::Initialize()`와 `Start()`를 호출한다.
4. `OnReceive()`에서 소비한 바이트 수를 반환한다.
5. `SendData()` 호출 시 세션 인덱스와 UID를 함께 전달한다.
6. 종료할 때 `End()`를 호출한다.

```cpp
#include "RIONetLite/server_net.h"

class GameNetEvents final : public rionetlite::INetEventManager {
 public:
  void OnConnect(std::int32_t session_index,
                 std::uint64_t session_uid) override {
    // 게임 로직 큐에 접속 이벤트를 전달한다.
  }

  std::uint32_t OnReceive(std::int32_t session_index,
                          std::uint64_t session_uid,
                          std::uint32_t data_size, char* data) override {
    // 완성된 패킷을 게임 로직 큐에 전달하고 소비한 바이트 수를 반환한다.
    return data_size;
  }
};
```

실제 초기화와 패킷 파싱 코드는 `samples/echo_server`와 `samples/chat_server`를 참고한다.

## 네트워크 이벤트 처리 원칙

- 이벤트 콜백은 네트워크 스레드에서 실행되므로 오래 걸리는 게임 로직을 직접 수행하지 않는다.
- 패킷 처리나 DB 작업은 별도 게임 로직 워커에 전달한다.
- 동일한 세션의 `OnReceive`와 `OnClose`는 같은 I/O 워커에서 호출되지만 `OnConnect`는 다른 스레드에서 호출될 수 있다.
- 세션 인덱스는 재사용될 수 있으므로 송신과 강제 종료에는 반드시 현재 UID를 함께 사용한다.
- `OnReceive`는 실제로 소비한 바이트 수를 정확히 반환해야 한다.

## 코딩 스타일

[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)를 따르며 저장소의 `.clang-format`을 사용한다.

```powershell
$files = @(rg --files RIONetLite samples/echo_server samples/chat_server -g '*.h' -g '*.cpp')
clang-format -i --style=file -- $files
```

## 참고 자료

- [Registered I/O로 Windows 고성능 서버 만들기](https://docs.google.com/presentation/d/11PE5zKPtgCoxwfhNBWI1nM6gTuLRwWteZTQuX9bIHmM/edit?usp=sharing)

## 향후 작업

- 네트워크 이벤트와 게임 패킷 처리 워커의 완전한 분리
- 반복 접속·강제 종료 및 장시간 부하 테스트
- 메모리 할당자와 병렬 큐 성능 비교
- 자동화된 단위·통합·부하 테스트 추가
