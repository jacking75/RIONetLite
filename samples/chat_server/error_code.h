#pragma once

#include <cstdint>

// 101 ~ 
enum class ErrorCode : uint16_t
{
	kNONE = 0,

	kPACKET_SIZE_TOO_LARGE = 201
};