#pragma once

#include "define.h"


namespace c2rionet
{
	class INetEventManager
	{
	public:
		INetEventManager() = default;
		virtual ~INetEventManager() = default;

		virtual void OnConnect([[maybe_unused]] const INT32 index_, 
			[[maybe_unused]] const UINT64 uniqueId_)
		{
		}

		virtual void OnClose([[maybe_unused]] const INT32 index_, 
			[[maybe_unused]] const c2rionet::DisconnectReason reason_)
		{
		}

		virtual UINT32 OnReceive([[maybe_unused]] const INT32 index_, 
			[[maybe_unused]] const UINT64 uniqueId_, 
			[[maybe_unused]] const UINT32 dataSize_, 
			[[maybe_unused]] const char* pData_)
		{
			return 0;
		}

	
	};
}