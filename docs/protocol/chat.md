# Chat wire protocol version 1

모든 정수는 big-endian이다. 문자열은 strict UTF-8이고 길이는 문자 수가 아니라 byte 수다. 전체 frame은 8..1024바이트다. 아래 계약은 서버와 .NET 10 클라이언트의 공통 fixture로 검증한다.

| offset | 크기 | 값 |
|---:|---:|---|
| 0 | 4 | u32 전체 길이, header 포함이다. |
| 4 | 2 | u16 version=1이다. |
| 6 | 2 | u16 type이다. |
| 8 | 나머지 | payload다. |

| 요청 type | 상태 | payload | 응답 |
|---:|---|---|---|
| 1 login | Login | u16 이름 byte 길이 + UTF-8, 1..32 byte | 101, 빈 payload다. |
| 2 join | Lobby | u32 room 번호다. 0도 허용한다. | 102, 같은 room 번호다. |
| 3 chat | Room | u16 text byte 길이 + UTF-8, 0..256 byte | 같은 room의 모든 사용자에게 103, 동일 payload다. 발신자도 포함한다. |
| 4 heartbeat | 모든 상태 | 빈 payload다. | 응답은 없고 activity를 갱신한다. |

```text
login A: 00 00 00 0B 00 01 00 01 00 01 41
join 1:  00 00 00 0C 00 01 00 02 00 00 00 01
chat hi: 00 00 00 0C 00 01 00 03 00 02 68 69
beat:    00 00 00 08 00 01 00 04
```

잘못된 길이·version·type·상태·UTF-8은 protocol close다. 로그인은 이름 등록이며 비밀번호 인증이 아니다. room leave/list/password·메시지 저장·사용자 영속 ID는 없다. 다른 room으로 이동하려면 새 연결로 들어간다. 포화는 overload close를 사용한다.

[chat-fixtures.tsv](chat-fixtures.tsv)는 8개 request/response의 golden hex다. C++ unit과 C# 실제 encoder/decoder가 동일 파일을 읽는다. [Chat 사용법](../samples/chat-server.md)에서 다중 shard와 split/coalesce E2E를 실행한다.
