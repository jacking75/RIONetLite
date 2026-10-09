# 패킷 프레이밍

Echo의 LengthPrefixedDecoder는 big-endian u32 **header를 포함한 전체 길이**를 읽는다. ping 4바이트의 전체 길이는 8이다.

```text
00 00 00 08 | 70 69 6E 67
전체 길이 8 | p  i  n  g
```

| callback span | 소비량 | 결과 |
|---|---:|---|
| `00 00` | 0 | header 미완성이다. |
| `00 00 00 08 70` | 0 | 이전 미소비 바이트도 포함하며 body 미완성이다. |
| `00 00 00 08 70 69 6E 67` | 8 | 메시지 하나다. |
| 위 8바이트 두 개 | 16 | 메시지 두 개다. |
| 위 8바이트 뒤 다음 header 2바이트 | 8 | 미완성 header는 남긴다. |

decoder는 자체 누적 버퍼가 없다. OnData의 미소비 바이트는 transport가 다음 callback에 다시 포함한다. 애플리케이션이 또 누적하면 중복 처리된다. adapter는 raw 바이트를 모두 복사·소비하므로 게임 워커가 자기 누적 버퍼에서 framing한다. 두 누적 위치를 혼동하지 않는다.

길이 0~3은 kInvalidLength, maximum 초과는 kMessageTooLarge, 범위 안의 미완성은 정상 대기다. 오류 시 RequestClose(session, kProtocolError)를 요청한다. span보다 많은 소비량을 반환하면 연결이 닫힌다. decoder handler 예외는 전파되므로 네트워크 callback 경계에서 해당 세션을 닫는다.

Send(session, header, payload)는 두 span을 하나로 수락하고 복사한다. 전체 크기는 max_message_bytes 이하이어야 한다. header·byte order·version은 애플리케이션이 만든다. C#은 BinaryPrimitives의 BigEndian 함수를 사용한다. [Chat 명세](protocol/chat.md)의 fixture로 양쪽을 확인한다. LoadClient의 split/coalesce 시나리오로 실제 TCP를 검사한다.
