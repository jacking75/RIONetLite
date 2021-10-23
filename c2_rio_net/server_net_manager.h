#pragma once

#include <vector>
#include <thread>
#include <concurrent_queue.h>

#include ".\impl\server_net_manager.h"
#include ".\impl\helper.h"
#include "define.h"



namespace c2rionet
{	
	template<class TNetEventManager>
	class ServerNetManager
	{
	public:
		ServerNetManager() = default;
		~ServerNetManager() = default;
		
	
		NetErrorCode Initialize(NetConfig net_cfg, TNetEventManager* p_net_event_mgr) {
			return impl_.Initialize(net_cfg, p_net_event_mgr);
		}

		void StartAndBlocking() { 
			impl_.StartAndBlocking();
		}
		
		void End() { 
			impl_.End(); 
		}
		
		NetErrorCode SendData(const INT32 session_index, const UINT64 session_uid, const UINT32 data_size, const char* p_data) {
			return impl_.SendData(session_index, session_uid, data_size, p_data);
		}

		void ForcedClose(const INT32 session_index, const UINT64 session_uid) {
			impl_.ForcedClose(session_index, session_uid);
		}
		

	private:
		ServerNetManagerImpl<TNetEventManager> impl_;
	};

}
