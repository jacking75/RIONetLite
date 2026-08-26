# ADR 0001: 지원 플랫폼과 배포 단위

- 상태: 승인
- 날짜: 2026-08-26

## 결정

RIONetLite 1차 릴리스는 다음 환경을 지원한다.

- Windows x64 전용이다.
- Windows 10 1803 이상, Windows 11, Windows Server 2019 이상을 대상으로 한다.
- Visual Studio 2026, MSVC v145와 Windows SDK 10.0을 기준 도구 체인으로 사용한다.
- C++은 `/std:c++latest`, 런타임은 Release `/MD`, Debug `/MDd`를 사용한다.
- 전송 계층은 Registered I/O 기반 TCP/IPv4 서버다.
- 배포 기본 단위는 정적 라이브러리, 공개 헤더와 PDB다.
- RIO 함수 테이블을 얻지 못하면 일반 Winsock으로 자동 fallback하지 않고 구체적인 초기화 오류를 반환한다.
- mimalloc은 vcpkg로 제공하되 최종 executable이 적용 여부를 결정한다. 공개 ABI에 mimalloc 타입을 노출하지 않는다.

IPv6, TLS, UDP, 클라이언트 API와 DLL ABI는 별도 ADR 없이 1차 범위에 추가하지 않는다.

## 이유

현재 구현은 `VirtualAlloc2`, double mapping과 Windows RIO에 강하게 의존한다. 플랫폼 추상화를 먼저 넓히면 세션과 비동기 요청 수명 검증보다 범위가 커진다. 첫 안정 릴리스는 실제 게임 서버 대상 환경에 집중한다.

## 결과

- x86/Win32 프로젝트 구성은 만들지 않는다.
- 지원 범위 밖 OS는 빌드 성공과 무관하게 공식 검증 대상으로 표기하지 않는다.
- 플랫폼별 구현을 추가할 때 public API와 내부 transport 경계를 유지한다.
