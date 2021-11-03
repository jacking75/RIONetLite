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


	void Init(const std::int32_t room_num, const std::int32_t max_user_count);

	std::int32_t GetMaxClientCount() { return max_client_count_; }
		
	std::int32_t GetCurrentClientCount() { return current_client_count_; }
		
	std::int32_t GetRoomNumber() { return room_num_; }

	std::uint16_t EnterClient(ClientSession* p_client);
		
	void LeaveClient(ClientSession* p_client);
						
	void NotifyChat(std::int32_t client_index, const char* p_user_id, const char* p_msg);
		
		
	
private:
	void SendToAllUser(const std::uint16_t data_size, void* p_data, const std::int32_t pass_user_index, bool is_except_me);


	std::int32_t room_num_ = -1;

	std::list<ClientSession*> client_list_;
		
	std::int32_t max_client_count_ = 0;

	std::uint16_t current_client_count_ = 0;
};
