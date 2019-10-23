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

		~SessionManager()
		{
			for (auto pSession : mSessionList)
			{
				delete pSession;
			}			
		}

		bool PrepareSessionPool(ConnectionNetConfig netConfig)
		{
			//CRASH_ASSERT(LIoThreadId == MAIN_THREAD_ID);
			int client_index = 0;

			for (int i = 1; i <= MAX_RIO_THREAD; ++i) //TODO: 모든 상수 모두 설정변수로 바꾸기
			{
				for (int j = 0; j < MAX_CLIENT_PER_RIO_THREAD; ++j)
				{
					auto client = new Session;
					client->Init(client_index, i);
					if (false == client->RioInitialize(netConfig)) { return false; }

					mSessionList.push_back(client);
					mFreeSessionList[i].push_back(client_index);
					++client_index;
				}
			}

			max_session_count_ = client_index;
			return true;
		}

		Session* GetSessionObj(const unsigned int session_index)
		{
			if (session_index >= max_session_count_) { return nullptr; }
			return mSessionList[session_index];
		}
		Session* IssueClientSession()
		{
			FastSpinlockGuard guard(mLock);

			uint64_t threadId = (mCurrentIssueCount++ % MAX_RIO_THREAD) + 1;
			CRASH_ASSERT(threadId > 0);

			CRASH_ASSERT((mCurrentIssueCount - mCurrentReturnCount) <= (MAX_RIO_THREAD * MAX_CLIENT_PER_RIO_THREAD));

			if (mFreeSessionList[threadId].empty()) { return false; }

			auto index = mFreeSessionList[threadId].front();
			mFreeSessionList[threadId].pop_front();
			auto newSession = GetSessionObj((unsigned int)index);

			if (newSession == nullptr) { return nullptr; }
			
			return newSession;
		}

		//TODO: ClientSession* 가 아닌 index
		void ReturnClientSession(Session* client)
		{
			FastSpinlockGuard guard(mLock);

			//CRASH_ASSERT(client->mSocket == NULL && client->mConnected == false && client->mRefCount == 0);

			uint64_t threadId = (mCurrentReturnCount++ % MAX_RIO_THREAD) + 1;
			CRASH_ASSERT(threadId > 0);

			mFreeSessionList[threadId].push_back(client->GetIndex());
		}


	private:
		unsigned int max_session_count_ = 0;
		//typedef std::vector<ClientSession*> ClientList;
		std::deque<int>	mFreeSessionList[MAX_RIO_THREAD + 1];
		std::vector<Session*> mSessionList;
		//ClientList	mOccupiedSessionList;

		FastSpinlock mLock;

		uint64_t mCurrentIssueCount = 0;
		uint64_t mCurrentReturnCount = 0;

	};
}