#pragma once

#include <vector>
#include <thread>
#include <concurrent_queue.h>

#include ".\impl\server_net_manager_impl.h"
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
		
		// 이 라이브러리르 사용하는 사람은 public 부분만을 보기 때문에 함수 이름을 보기 편하게 하기 위해 구현은 다른 함수로 분리했다.

		NetErrorCode Initialize(NetConfig net_cfg, TNetEventManager* p_net_event_mgr)
		{
			return impl_.Initialize(net_cfg, p_net_event_mgr);
		}

		void StartAndBlocking() { 
			impl_.StartAndBlocking();
		}
		
		void End() { 
			impl_.End(); 
		}
		
		const RIO_CQ& GetCompletionQueue(int thread_id) { 
			return impl_.GetCompletionQueue(thread_id); 
		}


		NetErrorCode SendData(const INT32 session_index, const UINT64 session_uid, const UINT32 data_size, const char* p_data)
		{
			return impl_.SendData(session_index, session_uid, data_size, p_data);
		}

		void ForcedClose(const INT32 session_index, const UINT64 session_uid)
		{
			impl_.ForcedClose(session_index, session_uid);
		}
		

	private:
		ServerNetManagerImpl<TNetEventManager> impl_;



		/*NetConfig net_config_;

		TNetEventManager* net_event_mgr_;
				
		bool is_run_io_work_thread_ = false;
		std::vector<std::thread> io_work_threads_;

		std::vector<RIO_CQ> rio_completion_queues_;
		
		SOCKET	listen_socket_ = INVALID_SOCKET;

		SessionManager session_mgr_;

		UINT64 connect_sequence_id_ = 0;

		std::vector<concurrency::concurrent_queue<LazyedDisConnectSession>> lazyed_dis_connect_sessions_;	*/
	};

}
