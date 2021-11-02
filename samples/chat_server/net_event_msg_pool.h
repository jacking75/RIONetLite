#pragma once

#include "net_event_msg.h"

#include <vector>


class NetEventMsgObjectPool
{
public:
	NetEventMsgObjectPool() = default;
	~NetEventMsgObjectPool() = default;

	void Init(const int max_session_count, const int max_msg_count_per_session)
	{
		max_session_count_ = max_session_count;
		max_msg_count_per_session_ = max_msg_count_per_session;

		for (int i = 0; i < max_session_count; ++i) {
			session_buffers_.emplace_back();
			
			for (int j = 0; j < max_msg_count_per_session; ++j) {
				session_buffers_[i].emplace_back();				
			}

			session_cur_pos_.push_back(0);
		}
	}

	void ClearSession(const int session_index)
	{
		session_cur_pos_[session_index] = 0;
	}

	NetEventMsg* Alloc(const int session_index)
	{
		if (session_cur_pos_[session_index] >= max_msg_count_per_session_) {
			session_cur_pos_[session_index] = 0;
		}

		auto index = session_cur_pos_[session_index];
		auto p_data = &session_buffers_[session_index][index];
		++session_cur_pos_[session_index];

		p_data->session_index_ = session_index;
		return p_data;
	}

private:
	std::vector<std::vector<NetEventMsg>> session_buffers_;
	std::vector<int> session_cur_pos_;

	int max_session_count_;	
	int max_msg_count_per_session_;
};

