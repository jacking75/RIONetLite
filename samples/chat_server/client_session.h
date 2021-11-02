#pragma once

#include "Packet.h"

#include <string>
#include <cstdint>

class ClientSession
{
public:
	enum class DOMAIN_STATE 
	{
		kNONE = 0,
		kLOGIN = 1,
		kROOM = 2
	};


	ClientSession() = default;
	~ClientSession() = default;


	void Init(const int32_t index)
	{
		index_ = index;
	}

	void Clear()
	{
		room_index_ = -1;
		user_id_ = "";
		is_confirm_ = false;
		cur_domain_state = DOMAIN_STATE::kNONE;
	}

	int SetLogin(char* login_id)
	{
		cur_domain_state = DOMAIN_STATE::kLOGIN;
		user_id_ = login_id;

		return 0;
	}
		
	void EnterRoom(int32_t roomIndex)
	{
		room_index_ = roomIndex;
		cur_domain_state = DOMAIN_STATE::kROOM;
	}
		
	void SetDomainState(DOMAIN_STATE val) 
	{ 
		cur_domain_state = val; 
	}

	int32_t GetCurrentRoom()
	{
		return room_index_;
	}

	int32_t GetNetConnIdx()
	{
		return index_;
	}

	std::string GetUserId() const
	{
		return  user_id_;
	}

	DOMAIN_STATE GetDomainState() 
	{
		return cur_domain_state;
	}
				


private:
	int32_t room_index_ = -1;

	std::string user_id_;
	bool is_confirm_ = false;
	std::string auth_token;

	int32_t index_ = -1;

	DOMAIN_STATE cur_domain_state = DOMAIN_STATE::kNONE;		
};
