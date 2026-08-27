# Changelog

## 0.2.0 - 2026-08-27

- `include/rionetlite` 공개 API, generation 기반 `SessionId`, 유계 송수신 및 이벤트 큐를 추가했다.
- polling/adaptive/notification RIO worker 정책, 구조화 로그와 메트릭을 추가했다.
- EchoServer, shard 기반 ChatServer, IOCP LoadClient와 단위·통합·스트레스·벤치마크 자동화를 추가했다.
- `RIONetLite/server_net.h` 호환 API는 0.2.x에서만 유지하고 0.3.0에서 제거할 예정이다.

이 프로젝트는 0.x 동안 ABI 호환을 보장하지 않는다. 공개 API 폐기는 최소 한 minor 릴리스 동안 deprecated 상태를 거친다.
