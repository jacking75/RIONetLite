# 개발 참여

Visual Studio 2026의 C++ 데스크톱 구성, MSVC v145, Windows SDK와 manifest-mode vcpkg를 준비한다. x64 Debug/Release, `/std:c++latest`, `/utf-8`을 유지한다. 외부 코드와 사용자별 설정·바이너리는 변경 범위에 넣지 않는다.

```powershell
tools\check-format.ps1
tools\build.ps1 -Configuration Debug -Rebuild
tools\build.ps1 -Configuration Release -Rebuild
tools\check-docs.ps1
git diff --check
```

네트워크 변경은 실제 RIO 통합 검사와 관련 split/coalesce/close/overload 검사를 추가한다. 공개 헤더 변경은 예제·함수 계약·생성 API 목록을 함께 갱신한다. `tools\test-chat.ps1`, `tools\run-memory-safety.ps1`, `tools\run-stress.ps1`은 해당 동작의 로컬 검증 도구다. CI 작업은 유지보수자의 명시적인 요청이 있어야 수행한다.

버그 보고에는 버전·source hash·OS·compiler/SDK·명령·seed·최소 재현·예상 결과와 실제 결과를 포함한다. 민감한 패킷이나 dump는 공개하지 않고 [보안 정책](SECURITY.md)의 비공개 경로를 이용한다. PR은 문제와 바뀐 동작을 먼저 설명하고 실행한 검사와 실행하지 않은 검사를 구분한다. 별도 PC·실제 사용자·운영 부하는 로컬 테스트로 대체하지 않는다.
