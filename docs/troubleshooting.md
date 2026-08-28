# 문제 해결

- `RIO unavailable`: x64 Windows와 RIO 지원 Winsock provider인지 확인한다.
- `bind failed`: 동일 port 프로세스를 종료하거나 다른 `--port`를 쓴다. 테스트는 OS 배정 port를 사용한다.
- vcpkg restore 실패: Visual Studio 2026의 vcpkg 구성요소와 루트 `vcpkg.json` baseline을 확인한다.
- page alignment 오류: recv/send buffer를 `GetSystemInfo` page size 배수로 둔다.
- 높은 CPU: notification 또는 adaptive로 바꾸고 spin을 낮춘다.
- `kWouldBlock`: 배압 단위는 `send_buffer_bytes`다. 확대 전에 소비 속도, rate limit과 high-watermark를 조사한다.
- shutdown timeout: logger의 session/outstanding/native error와 dump를 보존한다. `remaining_outstanding`이 0이 아니면 등록 버퍼와 CQ를 의도적으로 누수시킨 상태이므로 프로세스를 재사용하지 않는다.
- 세션 슬롯 고갈: 죽은 클라이언트가 슬롯을 점유하는지 확인하고 `keepalive_time_ms`와 `idle_timeout_ms`를 설정한다.
- accept 거부 급증: `OnRejectedConnection`의 native error를 본다. 핸들 고갈이면 accept 스레드가 백오프하므로 CPU 대신 거부 카운터가 오른다.

임의 Sleep으로 타이밍을 숨기지 말고 seed, 설정, JSON과 resource CSV를 함께 남기는 것이 올바르다. ASan x64와 Application Verifier/PageHeap의 정확한 구성, 해제 명령과 판정 기준은 [메모리 안전성 검사](memory-safety.md)를 따른다.
