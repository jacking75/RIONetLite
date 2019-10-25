#pragma once
#include <vector>
#include <deque>

#include <WinSock2.h>

#include "Exception.h"
#include "FastSpinlock.h"
#include "Session.h"


namespace C2RIONetLib
{
	class SessionManager
	{
	public:
		SessionManager() = default;
		~SessionManager() = default;
		
		bool PrepareSessionPool(unsigned int sessionCountPerRioThread, NetConfig netConfig)
		{
			unsigned int client_index = 0;
			auto rioThreadCount = netConfig.IoWorkThreadCount;

			for (decltype(rioThreadCount) i = 0; i <= rioThreadCount; ++i)
			{
				for (decltype(sessionCountPerRioThread) j = 0; j < sessionCountPerRioThread; ++j)
				{
					auto client = new Session;
					client->Init(client_index, i);
					if (false == client->RioInitialize(netConfig.Conn)) { return false; }

					mSessions.push_back(client);
					mFreeSessionindexs.push_back(client_index);
					++client_index;
				}
			}

			max_session_count_ = client_index;
			return true;
		}

		void Dispose()
		{
			LogFuncPtr((int)LogLevel::Info, "SessionManager::Dispose - Start");

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

			LogFuncPtr((int)LogLevel::Info, "SessionManager::Dispose - Complete");
		}

		Session* GetSessionObj(const unsigned int session_index)
		{
			if (session_index >= max_session_count_) { return nullptr; }
			return mSessions[session_index];
		}

		Session* GetSessionObj(const unsigned int session_index, const UINT64 sessionUniqueId)
		{
			if (session_index >= max_session_count_) { return nullptr; }
			if (mSessions[session_index]->GetUniqueId() != sessionUniqueId) { return nullptr; }

			return mSessions[session_index];
		}

		Session* IssueClientSession()
		{
			FastSpinlockGuard guard(mLock);
						
			if (mFreeSessionindexs.empty()) { return false; }

			auto index = mFreeSessionindexs.front();
			auto newSession = GetSessionObj(index);
						
			mFreeSessionindexs.pop_front();
			
			//newSessions는 절대 null 객체가 아니다!!!
			return newSession;
		}

		void ReturnSession(const int sessionIndex)
		{
			FastSpinlockGuard guard(mLock);						
			mFreeSessionindexs.push_back(sessionIndex);
		}

		

	private:
		FastSpinlock mLock;

		unsigned int max_session_count_ = 0;
		uint64_t mCurrentIssueCount = 0;
		uint64_t mCurrentReturnCount = 0;

		std::deque<unsigned int> mFreeSessionindexs;
		std::vector<Session*> mSessions;
	};
}