#pragma once

#include "client_session.h"

#include <functional>
#include <cstdint>


class Room 
{
public:
	Room() = default;
	~Room() = default;

	std::function<void(std::int32_t, void*, std::int16_t)> SendPacketFunc;


	void Init(const std::int32_t roomNum, const std::int32_t maxUserCount);

	std::int32_t GetMaxUserCount() { return max_client_count_; }
		
	std::int32_t GetCurrentUserCount() { return current_user_count; }
		
	std::int32_t GetRoomNumber() { return room_num_; }

	std::uint16_t EnterUser(ClientSession* pUser);
		
	void LeaveUser(ClientSession* pUser);
						
	void NotifyChat(std::int32_t connIndex, const char* UserID, const char* Msg);
		
		
	
private:
	void SendToAllUser(const std::uint16_t dataSize, void* pData, const std::int32_t passUserindex, bool exceptMe);


	std::int32_t room_num_ = -1;

	std::list<ClientSession*> client_list_;
		
	std::int32_t max_client_count_ = 0;

	std::uint16_t current_user_count = 0;
};
