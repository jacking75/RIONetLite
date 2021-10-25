#pragma once

#include "define.h"

#include <cstdint>


namespace c2rionet
{
	class INetEventManager
	{
	public:
		INetEventManager() = default;
		virtual ~INetEventManager() = default;

		virtual void OnConnect([[maybe_unused]] const std::int32_t index_, 
								[[maybe_unused]] const std::uint64_t uniqueId_)
		{
		}

		virtual void OnClose([[maybe_unused]] const std::int32_t index_, 
							[[maybe_unused]] const c2rionet::DisconnectReason reason_)
		{
		}

		virtual std::uint32_t OnReceive([[maybe_unused]] const std::int32_t index_,
										[[maybe_unused]] const std::uint64_t uniqueId_,
										[[maybe_unused]] const std::uint32_t dataSize_,
										[[maybe_unused]] const char* pData_)
		{
			return 0;
		}

	
	};
}