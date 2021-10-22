#pragma once
#include <vector>
#include <deque>

#include <WinSock2.h>

#include "exception.h"
#include "fast_spin_lock.h"
#include "session.h"


namespace c2rionet
{
	const UINT64 RE_USE_SESSION_WAITE_TIMESEC = 3;

	struct SessionIndexContext
	{
		UINT32 Index = 0;
		UINT64 ReUseAbleTimeSec = 0;
	};

	class SessionManager
	{
	public:
		SessionManager() = default;
		~SessionManager() = default;
		
		bool PrepareSessionPool(unsigned int sessionCountPerRioThread_, NetConfig netConfig_)
		{
			unsigned int client_index = 0;
			auto rioThreadCount = netConfig_.io_work_thread_count_;

			for (decltype(rioThreadCount) i = 0; i <= rioThreadCount; ++i)
			{
				for (decltype(sessionCountPerRioThread_) j = 0; j < sessionCountPerRioThread_; ++j)
				{
					auto client = new SenderSession;
					client->Init(client_index, i);
					if (false == client->RioInitialize(netConfig_.conn_cfg_)) { return false; }

					mSessions.push_back(client);
					mFreeSessionindexs.emplace_back(SessionIndexContext{ client_index, 0 });
					++client_index;
				}
			}

			mMaxSessionCount = client_index;
			return true;
		}

		void Dispose()
		{
			LogFuncPtr((int)LogLevel::info, "SessionManager::Dispose - Start");

			for (auto pSession : mSessions)
			{
				if (pSession->IsConnected())
				{
					auto uniqueId = pSession->GetUniqueId();
					pSession->Disconnect(uniqueId); // 네트워크 라이브러리 종료 시에 호출
				}

				pSession->Dispose();
				delete pSession;
			}

			LogFuncPtr((int)LogLevel::info, "SessionManager::Dispose - Complete");
		}

		SenderSession* GetSessionObj(const UINT32 sessionIndex_)
		{
			if (sessionIndex_ >= mMaxSessionCount) { return nullptr; }
			return mSessions[sessionIndex_];
		}

		SenderSession* GetSessionObj(const UINT32 sessionIndex_, const UINT64 sessionUniqueId_)
		{
			if (sessionIndex_ >= mMaxSessionCount) { return nullptr; }
			if (mSessions[sessionIndex_]->GetUniqueId() != sessionUniqueId_) { return nullptr; }

			return mSessions[sessionIndex_];
		}

		SenderSession* IssueClientSession(const UINT64 curSec_)
		{
			FastSpinlockGuard guard(mLock);

			if (mFreeSessionindexs.empty()) { return nullptr; }

			auto indexContext = mFreeSessionindexs.front();

			if (indexContext.ReUseAbleTimeSec > curSec_) { return nullptr; }

			auto newSession = GetSessionObj(indexContext.Index);
			mFreeSessionindexs.pop_front();

			//newSessions는 절대 null 객체가 아니다!!!
			return newSession;
		}

		void ReturnSession(const int sessionIndex_, const UINT64 curSec_)
		{
			FastSpinlockGuard guard(mLock);						
			mFreeSessionindexs.emplace_back(SessionIndexContext{ 
									(UINT32)sessionIndex_, 
									(curSec_ + RE_USE_SESSION_WAITE_TIMESEC) });
		}

		
		

	private:
		FastSpinlock mLock;

		UINT32 mMaxSessionCount = 0;
		UINT64 mCurrentIssueCount = 0;
		UINT64 mCurrentReturnCount = 0;

		std::deque<SessionIndexContext> mFreeSessionindexs;
		std::vector<SenderSession*> mSessions;
	};
}