#pragma once

#include <span>
#include <cstdint>


enum class NetEventType : std::int8_t
{
	kNONE = 0,
	kCONNECTION,
	kCLOSE,
	kONRECV
};

struct NetEventMsg
{
	int session_index_ = -1;
	NetEventType event_type_ = NetEventType::kNONE;
	std::span<char> data_;
	//unsigned int DataSize = 0;
	//char* pData = nullptr;
};