#pragma once

#include "Session.h"
#include <vector>
#include <thread>

namespace SendPacketCollectSystem
{
	class SendManager
	{
	public:
		SendManager() = default;
		~SendManager() = default;
		

		char* GetBuffer() { return mRingBuffer; }

		UINT32 EnableBufferSize() {	return mRingBufferSize;	}

		void MoveWriteablePos(const unsigned int pos_) { mWritePos = pos_; }

		UINT32 GetWriteablePos() { return mWritePos; }


		bool Start(const NetSendConfig netConfig_)
		{
			if ((netConfig_.MaxSessionCount % netConfig_.MaxSessionCount) != 0) { return false; }

			for (auto i = 0; i < netConfig_.MaxSessionCount; ++i)
			{
				m_Sessions.emplace_back();
				if (m_Sessions[i].Init(i, netConfig_) == false) { return false; }
			}

			auto sessionCountPerThread = netConfig_.MaxSessionCount / netConfig_.MaxWorkerThreadCount;

			StartThread(netConfig_.MaxWorkerThreadCount, sessionCountPerThread);
			return true;
		}

		void End()
		{
			mIsRunWorkThread = false;

			for (auto& thread : mWorkThreads)
			{
				if (thread.joinable())
				{
					thread.join();
				}
			}

			for (auto& session : m_Sessions)
			{
				session.Dispose();
			}
		}

		// 세션 객체의 재 사용은 이전 연결에서 최소한 몇초 이상은 지난 후라는 전제가 꼭 필요하다
		void ConnectSession(const UINT32 sessionIndex_, RIO_RQ requestQueue_)
		{
			// sessionIndex는 절대 범위를 벗어나지 않는다. 만약 벗어난다면 초기부터 모두 잘 못된 것임.
			m_Sessions[sessionIndex_].OnConnect(requestQueue_);			
		}

		void DisConnectSession(const UINT32 sessionIndex_)
		{
			m_Sessions[sessionIndex_].OnDisConnect();
		}

		bool PostSendSession(const UINT32 sessionIndex_, const UINT32 dataSize_, const char* pData_)
		{
			return m_Sessions[sessionIndex_].PostSend(dataSize_, pData_);
		}

		void SendCompletedSession(const UINT32 sessionIndex_, const UINT32 length_)
		{
			m_Sessions[sessionIndex_].SendCompleted(length_);
		}
		
				
	private:
		void StartThread(const UINT32 threadCount_, const UINT32 sessionCountPerThread_)
		{
			mIsRunWorkThread = true;
			
			UINT32 beginSessionIndex = 0;
			UINT32 endSessionIndex = sessionCountPerThread_;
			
			for (UINT32 i = 0; i < threadCount_; ++i)
			{
				mWorkThreads.emplace_back([threadIndex, this]() 
					{ 
						SendThread(beginSessionIndex, endSessionIndex);
					});

				std::this_thread::sleep_for(std::chrono::milliseconds(32));

				beginSessionIndex = endSessionIndex;
				endSessionIndex += sessionCountPerThread_;
			}
		}

		void SendThread(const UINT32 beginSessionIndex, const UINT32 endSessionIndex)
		{
			while (mIsRunWorkThread)
			{
				for (UINT32 i = beginSessionIndex; i < endSessionIndex; ++i)
				{
					auto* pSession = &m_Sessions[i];
					if (pSession->IsConnected() == false) { continue; }

					pSession->RealSend();
				}
			}
		}

				
		bool mIsRunWorkThread = false;		
		std::vector<std::thread> mWorkThreads;

		std::vector<Session> m_Sessions;		
	};
}