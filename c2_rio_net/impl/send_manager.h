#pragma once

#include "send_session.h"

#include <vector>
#include <thread>
#include <format>
#include <functional>

namespace c2rionet
{
	class SendManager
	{
	public:
		SendManager() = default;
		~SendManager() = default;
		
		std::function<void(const int32_t, const uint64_t, const DisconnectReason)> DisConnectFunc;


		bool Start(const NetSendConfig net_config)
		{
			if (net_config.max_worker_thread_count_ < 1 ||
				((net_config.max_session_count_ % net_config.max_worker_thread_count_) != 0)) {
				LogFuncPtr((int)LogLevel::kERROR, "SendManager::Start() - Fail Config");
				return false; 
			}

			for (uint32_t i = 0; i < net_config.max_session_count_; ++i) {
				auto p_session = new SendSession;
				if (p_session->Init(i, net_config) == false) {
					return false;
				}

				sessions_.push_back(p_session);				
			}

			auto sessionCountPerThread = net_config.max_session_count_ / net_config.max_worker_thread_count_;

			StartThread(net_config.max_worker_thread_count_, sessionCountPerThread);
			return true;
		}

		void End()
		{
			is_run_ = false;

			for (auto& thread : work_threads_) {
				if (thread.joinable()) {
					thread.join();
				}
			}

			for (auto session : sessions_) {
				session->Dispose();
				delete session;
			}
		}

		// 세션 객체의 재 사용은 이전 연결에서 최소한 몇초 이상은 지난 후라는 전제가 꼭 필요하다
		void OnConnectSession(const std::uint32_t session_index, const std::uint64_t session_uid,
						RIO_RQ request_queue)
		{
			// sessionIndex는 절대 범위를 벗어나지 않는다. 만약 벗어난다면 초기부터 모두 잘 못된 것임.
			sessions_[session_index]->OnConnect(session_uid, request_queue);
		}

		void OnCloseSession(const std::uint32_t session_index)
		{
			sessions_[session_index]->OnDisConnect();
		}

		bool PostSendSession(const std::uint32_t session_index, 
							[[maybe_unused]] const std::uint64_t session_uid,
							const std::uint32_t data_size, const char* p_data)
		{
			return sessions_[session_index]->PostSend(data_size, p_data);
		}

		void SendCompletedSession(const std::uint32_t session_index, 
								const std::uint32_t length)
		{
			sessions_[session_index]->SendCompleted(length);
		}
		
				
	private:
		void StartThread(const std::uint32_t thread_count, const std::uint32_t session_count_per_thread)
		{
			is_run_ = true;
			
			std::uint32_t beginSessionIndex = 0;
			std::uint32_t endSessionIndex = session_count_per_thread;
			
			for (std::uint32_t i = 0; i < thread_count; ++i)
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

		void SendThread(const std::uint32_t begin_session_index, const std::uint32_t end_session_index)
		{
			LogFuncPtr((int)LogLevel::kINFO, std::format("SendManager::SendThread() - Start. [begin_session_index:{0}] ~ (end_session_index:{1})", begin_session_index, end_session_index).c_str());

			while (is_run_) {
				for (std::uint32_t i = begin_session_index; i < end_session_index; ++i) {
					auto p_session = sessions_[i];

					if (p_session->IsConnected() == false) {
						continue; 
					}

					if (p_session->RealSend() == false) {
						DisConnectFunc(p_session->GetIndex(), p_session->GetUID(), DisconnectReason::kSEND_THREAD_REAL_SEND_ERROR);
					}
				}

				Sleep(1); ///< for low cpu-usage
			}

			LogFuncPtr((int)LogLevel::kINFO, std::format("SendManager::SendThread() - End. [begin_session_index:{0}] ~ (end_session_index:{1})", begin_session_index, end_session_index).c_str());
		}

				
		bool is_run_ = false;		
		std::vector<std::thread> work_threads_;

		std::vector<SendSession*> sessions_;		
	};
}