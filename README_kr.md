# RIONetLite

[English](README.md)

[단일 HTML 기술서](docs/rionetlite_book.html)는 B5 인쇄 기준 397쪽, 40개 장으로 구성했다. 그림 101개·애니메이션 40개·인터랙티브 실습 8개와 실제 소스 49개 파일의 해설을 담았다. HTML 파일 하나를 내려받아 브라우저로 열면 된다. 모든 그림과 스크립트가 파일 안에 들어 있다.

Windows Registered I/O(RIO)를 이용한 x64 C++ 게임 서버용 정적 라이브러리다. 현재 0.3.x는 preview이며 0.x API/ABI 호환은 보장하지 않는다. TCP/IPv4 서버만 제공하며 IPv6·TLS·UDP·클라이언트 transport API는 범위 밖이다. VS2026/v145·Windows SDK가 기준이다. Windows 10 1803+·11·Server 2019+는 대상 OS이며 전체 조합 검증 완료를 뜻하지 않는다.

[설치와 첫 Echo](docs/getting-started.md)의 터미널 두 개 실습부터 시작한다. 도구 설치·최초 의존성 복원은 별도 시간이 필요하다.

```powershell
tools\build.ps1 -Configuration Debug -Rebuild
bin\x64\Debug\EchoServer.exe --port 32452
```

다른 터미널에서 다음을 실행한다.

```powershell
bin\x64\Debug\LoadClient.exe --port 32452 --connections 4 --duration 10 --scenario echo --json result.json
```

exit 0·received_packets>0·연결 실패/data error/dropped packet/timeout 0을 확인한다. 서버에서 quit+Enter 또는 Ctrl+C로 종료하고 exit 0을 확인한다.

[설치·Echo 실습](docs/getting-started.md) → [API 레퍼런스](docs/api/README.md) → [Chat 실습](docs/samples/chat-server.md) 순서로 읽는다. 내부 동작은 [구조와 설계](docs/architecture.md), 설정·배압·종료·문제 해결은 [운영 안내](docs/operations.md)를 따른다.

callback span을 반환 뒤 저장하지 않는다. 게임·DB·파일 처리는 owned 유계 queue로 넘긴다. SessionId의 index와 generation을 함께 보관한다. Send(kQueued)는 복사·수락이며 상대 처리를 보장하지 않는다. callback 안의 같은 서버 Start/Stop은 거절하며 control thread에 종료를 요청한다. 객체 이동·파괴는 모든 application 호출자가 종료된 뒤 수행한다.

tools/package.ps1은 현재 source version을 읽고 정확한 snapshot에서 Release lib/PDB를 새로 빌드한다. compiler/SDK/runtime/source SHA256과 파일 해시를 기록하고 최종 ZIP을 다시 풀어 vcpkg 비활성 consumer를 빌드·실행한다. [ZIP 소비](docs/package-consumption.md)를 따른다. 정식 릴리스는 깨끗한 version tag와 -RequireReleaseTag 검사가 필요하다.

품질 검사는 [릴리스 체크리스트](docs/verification.md#릴리스-판정)에서 확인한다. C# ChatClient는 .NET 10 WinForms와 version 1 공유 protocol을 사용한다. 현재 로컬 자동 검사와 실제 GUI·별도 PC·신입 독자 평가는 구별한다. [개발 참여](CONTRIBUTING.md), [보안 보고](SECURITY.md), [외부 코드 고지](THIRD_PARTY_NOTICES.md)를 참고한다.

## 문서 안내

목적별 안내 여섯 개와 단일 HTML 기술서로 읽기 경로를 정리했다. 한국어 문서는 모두 아래에서 찾을 수 있다.

| 목적 | 문서 |
|---|---|
| 처음 실행하고 Echo를 이해한다 | [설치와 Echo 실습](docs/getting-started.md): 준비·TCP/RIO 입문·프레이밍·코드·CLI다. |
| 함수를 정확히 사용한다 | [공개 API 레퍼런스](docs/api/README.md): 헤더·호출 계약·결과·callback·자동 생성 symbol 목록이다. |
| 내부 구조를 이해한다 | [구조와 설계](docs/architecture.md): 스레드·세션 수명·플랫폼과 설계 결정이다. |
| 서버를 설정하고 운영한다 | [설정과 운영](docs/operations.md): 메모리 예산·배압·종료·로그·지표·문제 해결이다. |
| 게임 로직과 채팅을 구현한다 | [ChatServer와 C# 클라이언트](docs/samples/chat-server.md): UI 사용·게임 워커·다중 shard·wire protocol과 [공통 fixture](docs/protocol/chat-fixtures.tsv)다. |
| 변경과 배포를 검증한다 | [검증과 릴리스](docs/verification.md): 빌드·기능·stress·메모리 안전성·성능 측정·릴리스 판정이다. |
| 긴 설명과 실험으로 학습한다 | [단일 HTML 기술서](docs/rionetlite_book.html): 397쪽·그림·애니메이션·인터랙티브 실습·소스 해설이다. |

영문 자료는 [Getting Started](docs/getting-started-en.md)와 [Binary Package Consumption](docs/package-consumption.md)이다. 변경 이력은 [CHANGELOG](CHANGELOG.md)를 참고한다.
