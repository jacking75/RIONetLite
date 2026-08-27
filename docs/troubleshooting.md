# 문제 해결

- `RIO unavailable`: x64 Windows와 RIO 지원 Winsock provider인지 확인한다.
- `bind failed`: 동일 port 프로세스를 종료하거나 다른 `--port`를 쓴다. 테스트는 OS 배정 port를 사용한다.
- vcpkg restore 실패: Visual Studio 2026의 vcpkg 구성요소와 루트 `vcpkg.json` baseline을 확인한다.
- page alignment 오류: recv/send buffer를 `GetSystemInfo` page size 배수로 둔다.
- 높은 CPU: notification 또는 adaptive로 바꾸고 spin을 낮춘다.
- `kWouldBlock`: send buffer 확대 전에 소비 속도, rate limit과 high-watermark를 조사한다.
- shutdown timeout: logger의 session/outstanding/native error와 dump를 보존한다.

임의 Sleep으로 타이밍을 숨기지 말고 seed, 설정, JSON과 resource CSV를 함께 남기는 것이 올바르다. ASan x64와 Application Verifier/PageHeap의 정확한 구성, 해제 명령과 판정 기준은 [메모리 안전성 검사](memory-safety.md)를 따른다.
