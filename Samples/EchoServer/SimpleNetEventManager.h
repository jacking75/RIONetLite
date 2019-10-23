#pragma once

#include "../../C2RIONetLib/CommonDef.h"

#include <iostream>

class SimpleNetEventManager
{
public:
	SimpleNetEventManager() = default;
	~SimpleNetEventManager() = default;

	void OnConnect(const int index)
	{
		printf("[OnConnect] - SessionIndex(%d)\n", index);
	}

	void OnClose(const int index, const C2RIONetLib::CloseReason reason)
	{
		printf("[OnClose] - SessionIndex(%d), Reason(%d)\n", index, (int)reason);
	}

	void OnReceive(const int index, const unsigned int dataSize, const char* pData)
	{
		printf("[OnReceive] - SessionIndex(%d), size(%d)\n", index, (int)dataSize);
	}
};
