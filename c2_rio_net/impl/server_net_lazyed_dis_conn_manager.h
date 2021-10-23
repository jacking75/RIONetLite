#pragma once

#include "../define.h"

#include <vector>
#include <concurrent_queue.h>

namespace c2rionet
{
	class ServerNetLazyedDisConnManager
	{
	public:
		ServerNetLazyedDisConnManager() = default;
		~ServerNetLazyedDisConnManager() = default;

		void AddDisConnSessionQueue()
		{
			lazyed_dis_connect_sessions_.emplace_back();
		}

		void AddSession(const int thread_index, const INT32 session_index, const UINT64 session_uid, DisconnectReason reason)
		{
			lazyed_dis_connect_sessions_[thread_index].push({
					session_index, session_uid, reason
				});
		}

		LazyedDisConnectSession FetchDisConnSession(const int thread_index)
		{
			LazyedDisConnectSession disConnectInfo;
			lazyed_dis_connect_sessions_[thread_index].try_pop(disConnectInfo);
			return disConnectInfo;
			
		}

	private:
		std::vector<concurrency::concurrent_queue<LazyedDisConnectSession>> lazyed_dis_connect_sessions_;
	};

	
}