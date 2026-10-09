# 문제 해결

실패한 expected의 code/native_code/field/message와 실행 command·OS·compiler·source hash를 먼저 보존한다.

| 증상 | 확인·조치 | 재검증 |
|---|---|---|
| MSBuild/v145 없음 | VS2026 C++ workload와 v145 설치, Developer PowerShell을 확인한다. | tools/build.ps1 Debug와 Release다. |
| vcpkg 복원 실패 | root manifest와 MSBuild integration·네트워크·복원 오류를 확인한다. | 첫 실패 원인을 수정한 뒤 같은 빌드다. |
| bind_failed/native 10048 | Get-NetTCPConnection -LocalPort 32452 -State Listen으로 PID를 확인한다. 다른 프로세스를 무작정 종료하지 않고 다른 포트를 두 프로그램에 지정한다. | LoadClient echo다. |
| rio_unavailable | x64·대상 OS·Winsock provider와 native code를 확인한다. fallback transport는 없다. | 실제 integration 실행이다. |
| kWouldBlock 증가 | 소비 속도·send_buffer_bytes·queued_send_bytes와 sent_bytes를 확인한다. 무한 callback retry를 제거한다. | 같은 입력과 유계 retry/close 정책이다. |
| 정상 Chat 입력이 닫힘 | version/length/type/UTF-8 byte 수·close reason·event rejections를 확인한다. 기존 5바이트 클라이언트는 호환되지 않는다. | tools/test-chat.ps1의 split/coalesce·동일 room 검사다. |
| 다른 shard에 채팅 없음 | 모든 shard의 broadcast inbox 전달과 같은 room 설정을 확인한다. | 1/2/4 shard의 3명 이상 검사다. |
| stale session | index/generation 전체를 task에 저장한다. 이전 generation은 폐기한다. | slot 재사용 integration이다. |
| Stop 거절 | callback 안의 Stop을 제거하고 control thread에 flag를 보낸다. | callback reentry 검사다. |
| Stop timeout | outstanding·active·막힌 callback·game queue를 확인한다. 실패를 성공으로 감추지 않는다. | bounded callback과 같은 stop 모드에서 재실행한다. |
| 최초 workload 후 handle 증가 | RIO provider의 최초 worker 자원과 반복 추세를 구분한다. 같은 quiescent 상태를 비교한다. | 예열 포함 run-stress와 반복 Stop handle 회수다. |
| package link 실패 | manifest compiler/SDK/x64/MD, include/lib와 Ws2_32/OneCore를 확인한다. Release lib와 MDd를 섞지 않는다. | ZIP-only consumer rebuild다. |

public EventHandler에는 OnRejectedConnection callback이 없다. 거절 수는 rejected_connections 지표와 Logger로 확인한다. 기록 없는 오류를 임의 원인으로 단정하지 않는다. dump·payload 민감 정보는 [보안 경로](../SECURITY.md)로 전달한다.
