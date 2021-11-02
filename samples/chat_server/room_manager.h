#pragma once

#include "Room.h"
#include "error_code.h"

#include <cstdint>


class RoomManager
{
public:
	RoomManager() = default;
	~RoomManager() = default;


	void Init(const std::int32_t beginRoomNumber, const std::int32_t maxRoomCount, const std::int32_t maxRoomUserCount)
	{
		m_BeginRoomNumber = beginRoomNumber;
		m_MaxRoomCount = maxRoomCount;
		m_EndRoomNumber = beginRoomNumber + maxRoomCount;

		m_RoomList = std::vector<Room*>(maxRoomCount);

		for (auto i = 0; i < maxRoomCount; i++)
		{
			m_RoomList[i] = new Room();
			m_RoomList[i]->SendPacketFunc = SendPacketFunc;
			m_RoomList[i]->Init((i+ beginRoomNumber), maxRoomUserCount);
		}
	}

	std::int32_t GetMaxRoomCount() { return m_MaxRoomCount; }
		
	std::uint16_t EnterUser(std::int32_t roomNumber, ClientSession* pUser)
	{
		auto pRoom = GetRoomByNumber(roomNumber);
		if (pRoom == nullptr)
		{
			return (std::uint16_t)ErrorCode::kROOM_INVALID_INDEX;
		}


		return pRoom->EnterUser(pUser);
	}
		
	std::uint16_t LeaveUser(std::int32_t roomNumber, ClientSession* pUser)
	{
		auto pRoom = GetRoomByNumber(roomNumber);
		if (pRoom == nullptr)
		{
			return (std::uint16_t)ErrorCode::kROOM_INVALID_INDEX;
		}
			
		pUser->SetDomainState(ClientSession::DomainState::kLOGIN);
		pRoom->LeaveUser(pUser);
		return (std::uint16_t)ErrorCode::kNONE;
	}

	Room* GetRoomByNumber(std::int32_t number)
	{ 
		if (number < m_BeginRoomNumber || number >= m_EndRoomNumber)
		{
			return nullptr;
		}

		auto index = (number - m_BeginRoomNumber);
		return m_RoomList[index]; 
	} 

		
	std::function<void(std::int32_t, void*, std::int16_t)> SendPacketFunc;
		

private:
	std::vector<Room*> m_RoomList;
	std::int32_t m_BeginRoomNumber = 0;
	std::int32_t m_EndRoomNumber = 0;
	std::int32_t m_MaxRoomCount = 0;
};
