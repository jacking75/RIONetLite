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

[네트워크 입문](docs/networking-basics.md) → [Echo 코드](docs/tutorial/first-echo-server.md) → [프레이밍](docs/packet-framing.md) → [게임 워커](docs/tutorial/game-worker.md) → [Chat 실습](docs/samples/chat-server.md) 순서로 읽는다. 함수 사용은 [API 계약](docs/api/contracts.md), 운영은 [설정](docs/configuration.md)·[배압](docs/backpressure.md)·[종료](docs/graceful-shutdown.md)·[관측성](docs/observability.md)·[문제 해결](docs/troubleshooting.md)을 따른다.

callback span을 반환 뒤 저장하지 않는다. 게임·DB·파일 처리는 owned 유계 queue로 넘긴다. SessionId의 index와 generation을 함께 보관한다. Send(kQueued)는 복사·수락이며 상대 처리를 보장하지 않는다. callback 안의 같은 서버 Start/Stop은 거절하며 control thread에 종료를 요청한다. 객체 이동·파괴는 모든 application 호출자가 종료된 뒤 수행한다.

tools/package.ps1은 현재 source version을 읽고 정확한 snapshot에서 Release lib/PDB를 새로 빌드한다. compiler/SDK/runtime/source SHA256과 파일 해시를 기록하고 최종 ZIP을 다시 풀어 vcpkg 비활성 consumer를 빌드·실행한다. [ZIP 소비](docs/package-consumption.md)를 따른다. 정식 릴리스는 깨끗한 version tag와 -RequireReleaseTag 검사가 필요하다.

품질 검사는 [릴리스 체크리스트](docs/release-checklist.md)에서 확인한다. C# ChatClient는 .NET 10 WinForms와 version 1 공유 protocol을 사용한다. 현재 로컬 자동 검사와 실제 GUI·별도 PC·신입 독자 평가는 구별한다. [개발 참여](CONTRIBUTING.md), [보안 보고](SECURITY.md), [외부 코드 고지](THIRD_PARTY_NOTICES.md)를 참고한다.
