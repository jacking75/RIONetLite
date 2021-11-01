#pragma once

#include <cstdint>

enum class NetEventType : std::int8_t
{
	kNONE = 0,
	kCONNECTION,
	kCLOSE,
	kONRECV
};

class NetEventMsg
{
	int SessionIndex = -1;
	NetEventType EventType = NetEventType::kNONE;
	unsigned int DataSize = 0;
	char* pData = nullptr;

	void Set(const int session_index, const NetEventType type, const uint32_t data_size, char* p_data)
	{
		SessionIndex = session_index;
		EventType = type;
		DataSize = data_size;
		pData = p_data;
	}

	void SetReceive(const unsigned int dataSize_, char* pData_)
	{
		DataSize = dataSize_;
		pData = pData_;
	}
};