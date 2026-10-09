# 공개 API 레퍼런스

| 헤더 | 주요 API |
|---|---|
| `config.h` | `ServerConfig`, `ValidateServerConfig`, worker policy와 메모리 예산 |
| `error.h` | `Error`, `SendResult`, `CloseReason`, 안정된 문자열 변환 |
| `event_handler.h` | `SessionId`, `EventHandler`, 구조화 `Logger` |
| `server.h` | move-only `Server`, lifecycle, send/close/metrics |
| `packet_decoder.h` | 안전한 big-endian length-prefix decoder |
| `queued_event_adapter.h` | shard 유계 이벤트 전달과 포화 정책 |
| `metrics.h` | 인스턴스별 누적 metric snapshot |
| `version.h` | SemVer compile-time macro |

공개 symbol은 모두 `rionetlite` namespace에 있다. thread safety와 span lifetime은 각 헤더 주석 및 [스레딩 문서](../threading-model.md)를 따른다. `RIONetLite/` 경로는 전환 호환 계층이며 신규 코드에서 사용하지 않는다. 0.4.0에서 제거할 예정이다.

헤더의 `@api` 주석에서 생성한 전체 최상위 symbol 목록은 [generated.md](generated.md)다. `tools\generate-api-docs.ps1 -Check`는 새 공개 타입이나 함수를 주석·생성 문서에 반영하지 않으면 실패한다.

함수별 사전 조건·결과·스레드·수명·예외는 [contracts.md](contracts.md)에 있다. 생성 symbol 목록을 함수별 사용 설명의 대체로 취급하지 않는다.
