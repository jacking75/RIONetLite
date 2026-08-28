# 패킷 프레이밍

TCP는 메시지 경계를 보존하지 않는다. `LengthPrefixedDecoder`는 big-endian 32비트 전체 길이를 읽고 header 미만이면 기다리며, 최소 길이 미만 또는 설정 최대값 초과를 protocol error로 반환한다. raw pointer를 정수로 cast하거나 한 recv가 한 packet이라고 가정하는 코드는 잘못이다.

콜백은 완성된 모든 packet을 처리한 뒤 실제 소비 바이트 수를 반환한다. 반환값이 readable보다 크거나 0을 반복해 버퍼를 채우면 서버가 계약 위반 또는 과부하로 연결을 닫는다.

응답을 만들 때 header와 payload를 이어붙인 임시 buffer를 할당하는 것은 잘못이다. `Server::Send(session, header, payload)`는 두 span을 그대로 세션 송신 링에 이어 쓰므로, header는 스택 배열로 만들고 payload는 수신한 span을 그대로 넘기는 것이 올바르다. ChatServer의 `EncodeHeader`가 이 패턴이다.
