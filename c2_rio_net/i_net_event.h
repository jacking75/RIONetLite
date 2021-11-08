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

	// 아래의 네트워크 이벤트 함수에서는 처리를 빨리 끝내야 한다. 
	// 만약 지연이 발생하면 네트워크 처리용 스레드의 성능이 떨어질 수 있다.
	// OnConnect와 OnClose,OnReceive가 호출 되는 스레드는 서로 다르다.
	// 동일 세션이라면 OnClose와 OnReceive는 같은 스레드에서 호출된다
	virtual void OnConnect([[maybe_unused]] const std::int32_t index_, 
							[[maybe_unused]] const std::uint64_t uniqueId_)
	{
	}

	virtual void OnClose([[maybe_unused]] const std::int32_t index_, 
						[[maybe_unused]] const std::uint64_t uid, 
						[[maybe_unused]] const c2rionet::DisconnectReason reason_)
	{
	}

	virtual std::uint32_t OnReceive([[maybe_unused]] const std::int32_t index_,
									[[maybe_unused]] const std::uint64_t uniqueId_,
									[[maybe_unused]] const std::uint32_t dataSize_,
									[[maybe_unused]] char* pData_)
	{
		return 0;
	}

	
};
}// namespace c2rionet