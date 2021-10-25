#pragma once

#include "sendpacket_session.h"
#include <vector>
#include <thread>

namespace c2rionet
{
	class SendManager
	{
	public:
		SendManager() = default;
		~SendManager() = default;
		

		bool Start(const NetSendConfig net_config)
		{
			if ((net_config.max_session_count_ % net_config.max_session_count_) != 0) { return false; }

			for (auto i = 0; i < net_config.max_session_count_; ++i)
			{
				sessions_.emplace_back();
				if (sessions_[i].Init(i, net_config) == false) { return false; }
			}

			auto sessionCountPerThread = net_config.max_session_count_ / net_config.max_worker_thread_count_;

			StartThread(net_config.max_worker_thread_count_, sessionCountPerThread);
			return true;
		}

		void End()
		{
			is_run_work_thread_ = false;

			for (auto& thread : work_threads_)
			{
				if (thread.joinable())
				{
					thread.join();
				}
			}

			for (auto& session : sessions_)
			{
				session.Dispose();
			}
		}

		// 세션 객체의 재 사용은 이전 연결에서 최소한 몇초 이상은 지난 후라는 전제가 꼭 필요하다
		void ConnectSession(const UINT32 session_index, RIO_RQ request_queue)
		{
			// sessionIndex는 절대 범위를 벗어나지 않는다. 만약 벗어난다면 초기부터 모두 잘 못된 것임.
			sessions_[session_index].OnConnect(request_queue);			
		}

		void DisConnectSession(const UINT32 session_index)
		{
			sessions_[session_index].OnDisConnect();
		}

		bool PostSendSession(const UINT32 session_index, const UINT32 data_size, const char* p_data)
		{
			return sessions_[session_index].PostSend(data_size, p_data);
		}

		void SendCompletedSession(const UINT32 session_index, const UINT32 length)
		{
			sessions_[session_index].SendCompleted(length);
		}
		
				
	private:
		void StartThread(const UINT32 thread_count, const UINT32 session_count_per_thread)
		{
			is_run_work_thread_ = true;
			
			UINT32 beginSessionIndex = 0;
			UINT32 endSessionIndex = session_count_per_thread;
			
			for (UINT32 i = 0; i < thread_count; ++i)
			{
				work_threads_.emplace_back([beginSessionIndex, endSessionIndex, this]()
					{ 
						SendThread(beginSessionIndex, endSessionIndex);
					});

				std::this_thread::sleep_for(std::chrono::milliseconds(32));

				beginSessionIndex = endSessionIndex;
				endSessionIndex += session_count_per_thread;
			}
		}

		void SendThread(const UINT32 begin_session_index, const UINT32 end_session_index)
		{
			while (is_run_work_thread_)
			{
				for (UINT32 i = begin_session_index; i < end_session_index; ++i)
				{
					auto* pSession = &sessions_[i];
					if (pSession->IsConnected() == false) { continue; }

					pSession->RealSend();
				}
			}
		}

				
		bool is_run_work_thread_ = false;		
		std::vector<std::thread> work_threads_;

		std::vector<SendSession> sessions_;		
	};
}