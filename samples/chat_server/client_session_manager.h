#pragma once

#include "client_session.h"
#include "error_code.h"

#include <unordered_map>
#include <cstdint>


class ClientSessionManager
{

public:
	ClientSessionManager() = default;
	~ClientSessionManager() = default;

	void Init(const std::int32_t max_session_count)
	{
		max_session_count_ = max_session_count;
		session_obj_pool_ = std::vector<ClientSession*>(max_session_count_);

		for (auto i = 0; i < max_session_count_; ++i) {
			session_obj_pool_[i] = new ClientSession();
			session_obj_pool_[i]->Init(i);
		}
	}

	std::int32_t CurrentClientCount() 
	{ 
		return cur_session_count_; 
	}

	std::int32_t MaxClientCount() 
	{ 
		return max_session_count_; 
	}
		
	void IncreaseClientCount() 
	{ 
		++cur_session_count_; 
	}
		
	void DecreaseClientCount() 
	{
		if (cur_session_count_ > 0) {
			--cur_session_count_;
		}
	}

	ErrorCode AddClient(char* client_id, const int conn_idx)
	{
		//TODO 최흥배 유저 중복 조사하기

		auto client_idx = conn_idx;

		session_obj_pool_[client_idx]->SetLogin(client_id);
		session_dic_.insert(std::pair< char*, int>(client_id, conn_idx));

		return ErrorCode::kNONE;
	}
		
	std::int32_t FindClientIndexByID(char* client_id)
	{
		if (auto res = session_dic_.find(client_id); res != session_dic_.end()) {
			return (*res).second;
		}
			
		return -1;
	}
		
	void RemoveClient(ClientSession* p_remove_session)
	{
		session_dic_.erase(p_remove_session->GetUserId());
		p_remove_session->Clear();
	}

	ClientSession* GetClientByConnIdx(std::int32_t conn_idx)
	{
		return session_obj_pool_[conn_idx];
	}


private:
	std::int32_t max_session_count_ = 0;
	std::int32_t cur_session_count_ = 0;

	std::vector<ClientSession*> session_obj_pool_; //vector로
	std::unordered_map<std::string, std::int32_t> session_dic_;
};
