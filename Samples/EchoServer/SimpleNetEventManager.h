#pragma once

#include "../../C2RIONetLib/CommonDef.h"

#include <iostream>

class SimpleNetEventManager
{
public:
	SimpleNetEventManager() = default;
	~SimpleNetEventManager() = default;

	void OnConnect(const INT32 index, const UINT64 uniqueId)
	{
		printf("[OnConnect] - SessionIndex(%d), UniqueId(%I64u)\n", index, uniqueId);
	}

	void OnClose(const INT32 index, const C2RIONetLib::DisconnectReason reason)
	{
		printf("[OnClose] - SessionIndex(%d), Reason(%d)\n", index, (int)reason);
	}

	void OnReceive(const INT32 index, const UINT32 dataSize, const char* pData)
	{
		// 이 데이터는 방금 받은 데이터로 여기서 사용 도중 세션이 끊어져서 이 세션이 재 사용되면 버퍼의 값이 이상해질 수 있다. 이 이벤트 발생 후 RE_USE_SESSION_WAITE_TIMESEC는 보장한다
		UNREFERENCED_PARAMETER(pData);
		printf("[OnReceive] - SessionIndex(%d), size(%d)\n", index, (int)dataSize);
	}
};
