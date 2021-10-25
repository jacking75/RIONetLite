#pragma once
#include <vector>
#include <deque>

#include <WinSock2.h>

#include "exception.h"
#include "fast_spin_lock.h"
#include "tcp_session.h"


namespace c2rionet
{
	const UINT64 kRE_USE_SESSION_WAITE_TIME_SEC = 3;

	struct SessionIndexContext
	{
		UINT32 index_ = 0;
		UINT64 reuse_able_timesec_ = 0;
	};

	class SessionManager
	{
	public:
		SessionManager() = default;
		~SessionManager() = default;
		
		bool PrepareSessionPool(NetConfig net_cfg)
		{
			unsigned int client_index = 0;
			auto rioThreadCount = net_cfg.io_work_thread_count_;
			auto session_count_per_rio_thread = net_cfg.max_connection_count_ / net_cfg.io_work_thread_count_;

			for (decltype(rioThreadCount) i = 0; i <= rioThreadCount; ++i)
			{
				for (decltype(session_count_per_rio_thread) j = 0; j < session_count_per_rio_thread; ++j)
				{
					auto client = new TcpSession;
					client->Init(client_index, i);
					if (false == client->RioInitialize(net_cfg.conn_cfg_)) { return false; }

					sessions_.push_back(client);
					free_session_indexs_.emplace_back(SessionIndexContext{ client_index, 0 });
					++client_index;
				}
			}

			max_session_count_ = client_index;
			return true;
		}

		void Dispose()
		{
			LogFuncPtr((int)LogLevel::kINFO, "SessionManager::Dispose - Start");

			for (auto pSession : sessions_)
			{
				if (pSession->IsConnected())
				{
					auto uniqueId = pSession->GetUniqueId();
					pSession->Disconnect(uniqueId); // 네트워크 라이브러리 종료 시에 호출
				}

				pSession->Dispose();
				delete pSession;
			}

			LogFuncPtr((int)LogLevel::kINFO, "SessionManager::Dispose - Complete");
		}

		TcpSession* GetSessionObj(const UINT32 session_index)
		{
			if (session_index >= max_session_count_) { return nullptr; }
			return sessions_[session_index];
		}

		TcpSession* GetSessionObj(const UINT32 session_index, const UINT64 session_uid)
		{
			if (session_index >= max_session_count_) { return nullptr; }
			if (sessions_[session_index]->GetUniqueId() != session_uid) { return nullptr; }

			return sessions_[session_index];
		}

		TcpSession* IssueClientSession(const UINT64 cur_sec)
		{
			FastSpinlockGuard guard(lock_);

			if (free_session_indexs_.empty()) { return nullptr; }

			auto indexContext = free_session_indexs_.front();

			if (indexContext.reuse_able_timesec_ > cur_sec) { return nullptr; }

			auto newSession = GetSessionObj(indexContext.index_);
			free_session_indexs_.pop_front();

			//newSessions는 절대 null 객체가 아니다!!!
			return newSession;
		}

		void ReturnSession(const int session_index, const UINT64 cur_sec)
		{
			FastSpinlockGuard guard(lock_);						
			free_session_indexs_.emplace_back(SessionIndexContext{ 
									(UINT32)session_index, 
									(cur_sec + kRE_USE_SESSION_WAITE_TIME_SEC) });
		}



		NetErrorCode SendData(const INT32 session_index, const UINT64 session_uid, const UINT32 data_size, const char* p_data)
		{
			auto session = GetSessionObj(session_index, session_uid);
			if (session == nullptr)
			{
				return NetErrorCode::kSEND_INVALID_SESSION;
			}

			if (session->IsEnableNetwork() == false)
			{
				return NetErrorCode::kSEND_SESSION_NETWORK_DISABLE;
			}

			if (session->PostSend(data_size, p_data) == false)
			{
				return NetErrorCode::kSEND_IO_FAIL;
			}

			return NetErrorCode::kNONE;
		}

		int ForcedClose(const INT32 session_index, const UINT64 session_uid)
		{
			auto session = GetSessionObj(session_index, session_uid);
			if (session == nullptr)
			{
				return -1;
			}

			session->SetDisableNetwork();

			auto threadIndex = session->GetRioThreadId();
			return threadIndex;
		}
				
		

	private:
		FastSpinlock lock_;

		UINT32 max_session_count_ = 0;
		UINT64 current_issue_count_ = 0;
		UINT64 current_return_count_ = 0;

		std::deque<SessionIndexContext> free_session_indexs_;
		std::vector<TcpSession*> sessions_;
	};
}