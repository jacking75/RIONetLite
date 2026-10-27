# RIONetLite

Windows Registered I/O(RIO)를 사용하는 C++ 게임 서버 네트워크 라이브러리다. 세션 식별, 비동기 버퍼 수명과 종료 순서의 안전성을 우선한다. 0.3.x는 preview이며 0.x 동안 API·ABI가 바뀔 수 있다.

Windows x64·TCP/IPv4 서버를 지원한다. Visual Studio 2026, MSVC v145, `/std:c++latest`, UTF-8과 Windows SDK를 사용한다. Windows 10 1803+·Windows 11·Server 2019+는 목표 환경이며 전체 OS/provider 조합을 인증한 상태는 아니다. IPv6·TLS·UDP·클라이언트 전송 API는 제공 범위 밖이다.

## 시작한다

Visual Studio의 C++ 데스크톱 개발·Windows SDK·vcpkg 구성요소를 설치한 뒤 저장소 루트에서 실행한다. 최초 의존성 복원에는 네트워크가 필요하다.

```powershell
tools\build.ps1 -Configuration Debug -Rebuild
tools\build.ps1 -Configuration Release -Rebuild
bin\x64\Debug\EchoServer.exe --port 32452 --duration 30
```

[HTML 책을 내려받아 브라우저로 연다](docs/interactive_book/index.html). **28개 장·5개 부록·20개 설명용 실험**과 코드 탐험·검색·인쇄가 HTML 하나에 들어 있다. 읽을 때 서버·계정·인터넷·Python·Node가 필요하지 않다. 실제 네트워크 실습은 별도 실행 파일로 수행한다.

| 해결할 질문 | 읽을 곳 |
|---|---|
| 환경을 준비하고 첫 왕복을 확인한다 | [빌드](docs/interactive_book/index.html#chapter=build-and-run) · [Echo](docs/interactive_book/index.html#chapter=echo-walkthrough) |
| 함수를 정확히 사용한다 | [공개 API 계약](docs/interactive_book/index.html#chapter=api-reference) |
| RIO·메모리·수신·송신을 이해한다 | [요청과 완료](docs/interactive_book/index.html#chapter=rio-requests) · [등록 메모리](docs/interactive_book/index.html#chapter=registered-memory) · [부분 송신](docs/interactive_book/index.html#chapter=partial-send) |
| 게임 큐와 채팅을 구현한다 | [로직 큐](docs/interactive_book/index.html#chapter=logic-queue) · [Chat](docs/interactive_book/index.html#chapter=chat-server) |
| 종료·설정·진단을 다룬다 | [서버 종료](docs/interactive_book/index.html#chapter=server-shutdown) · [설정](docs/interactive_book/index.html#chapter=configuration) · [문제 해결](docs/interactive_book/index.html#chapter=troubleshooting) |
| 변경·측정·배포를 검증한다 | [검사](docs/interactive_book/index.html#chapter=testing-and-memory) · [측정](docs/interactive_book/index.html#chapter=benchmarking) · [패키징](docs/interactive_book/index.html#chapter=packaging-and-release) |

## 근거와 유지관리 방법을 확인한다

[이번 판의 실행 결과와 미실행 범위](docs/interactive_book/index.html#chapter=book-maintenance&section=maintenance-acceptance)를 구분해 읽는다. 과거 ADR·측정 기록은 책의 별도 부록에 보존했다. 브라우저 실험은 실제 Windows RIO를 실행하지 않는다.

```powershell
npm.cmd --prefix tools/book ci
tools\check-docs.ps1
tools\package.ps1
```

책 저작·검사는 Python 3.10+·Node.js 20+·npm·설치된 Edge를 사용한다. [재생성·검증 명령](docs/interactive_book/index.html#chapter=book-maintenance&section=maintenance-build)을 따른다. 패키지는 정확한 소스 snapshot으로 Release lib/PDB를 다시 만들고 ZIP만으로 consumer를 빌드·실행한다. 로컬 배포 후보 검사와 정식 release tag·외부 게시를 구분한다.

[English](README.md) · [개발 참여](CONTRIBUTING.md) · [보안 보고](SECURITY.md) · [변경 이력](CHANGELOG.md) · [외부 코드 고지](THIRD_PARTY_NOTICES.md) · [라이선스](LICENSE)
