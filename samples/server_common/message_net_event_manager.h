#pragma once

#include "../../c2_rio_net/define.h"

#include <vector>
#include <concurrent_queue.h>


namespace servercommon
{
	enum class NetEventType : INT8
	{
		kNONE = 0,
		kCONNECTION,
		kCLOSE,
		kONRECV
	};

	struct NetEvent
	{		
		int SessionIndex = -1;
		NetEventType EventType = NetEventType::kNONE;
		unsigned int DataSize = 0;
		char* pData = nullptr;
		
		void Set(const int sessionIndex_, const NetEventType type_, const unsigned int dataSize_, char* pData_)
		{
			SessionIndex = sessionIndex_;
			EventType = type_;
			DataSize = dataSize_;
			pData = pData_;
		}

		void SetReceive(const unsigned int dataSize_, char* pData_)
		{
			DataSize = dataSize_;
			pData = pData_;
		}		
	};

	class NetEventRecvRingBuffer
	{
	public:
		NetEventRecvRingBuffer() = default;
		~NetEventRecvRingBuffer() = default;

		void Init(const int sessionIndex_, const unsigned int maxCount_)
		{
			mMaxCount = maxCount_;

			for (int i = 0; i < maxCount_; ++i)
			{
				mObjects.emplace_back(sessionIndex_,  NetEventType::kONRECV, 0, nullptr);
			}
		}

		void Clear()
		{
			mCurBufferPos = 0;
		}		

		NetEvent* GetData()
		{
			if (mCurBufferPos >= mMaxCount) { mMaxCount = 0; }

			return &mObjects[mCurBufferPos];
		}

	private:
		std::vector<NetEvent> mObjects;
		unsigned int mMaxCount;
		unsigned int mCurBufferPos;
	};

	class SessionNetEvent
	{
	public:
		SessionNetEvent() = default;
		~SessionNetEvent() = default;

		void Init(const int sessionIndex_, const unsigned int maxRecvEvnetCount_)
		{
			mConnect.Set(sessionIndex_, NetEventType::kCONNECTION, 0, nullptr);
			mConnect.Set(sessionIndex_, NetEventType::kCLOSE, 0, nullptr);
			mRecvBuffer.Init(sessionIndex_, maxRecvEvnetCount_);
		}

		NetEvent* GetConnect() { return &mConnect; }
		NetEvent* GetClose() { return &mClose; }
		NetEvent* GetRecv() { return mRecvBuffer.GetData(); }
		
	private:
		NetEvent mConnect;
		NetEvent mClose;
		NetEventRecvRingBuffer mRecvBuffer;
	};

	class MessageNetEventManager
	{
	public:
		MessageNetEventManager() = default;
		~MessageNetEventManager() = default;

		void OnConnect(const int index_)
		{
		}

		void OnClose(const int index_, const c2rionet::DisconnectReason reason_)
		{
		}

		void OnReceive(const INT32 index_, const UINT32 dataSize_, const char* pData_)
		{
		}


		int Init(const unsigned int threadCount_, const unsigned int sessionCount_, const unsigned int maxRecvEvnetCount_)
		{
			mLogicIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, threadCount_);
			if (mLogicIOCP == INVALID_HANDLE_VALUE)
			{
				return -1;
			}

			for (UINT32 i = sessionCount_; i < sessionCount_; ++i)
			{
				SessionNetEvent session;
				session.Init(i, maxRecvEvnetCount_);
				mSessionNetEventList.push_back(session);
			}

			return 0;
		}

		void Dispose()
		{
			if (mLogicIOCP != INVALID_HANDLE_VALUE) { CloseHandle(mLogicIOCP); }
		}

		void PostConnectEventMsg()
		{

		}

		void PostCloseEventMsg()
		{

		}

		void PostRecvEventMsg(const int dataSize_, const char* pData_)
		{

		}
		/*
		bool ProcessNetworkMessage(OUT INT8& msgOperationType, OUT INT32& connectionIndex, char* pBuf, OUT INT16& copySize, const INT32 waitTimeMillisec)
		{
			Message* pMsg = nullptr;
			Connection* pConnection = nullptr;
			DWORD ioSize = 0;
			auto waitTime = waitTimeMillisec;

			if (waitTime == 0)
			{
				waitTime = INFINITE;
			}

			if (auto result = GetQueuedCompletionStatus(
				m_hLogicIOCP,
				&ioSize,
				reinterpret_cast<PULONG_PTR>(&pConnection),
				reinterpret_cast<LPOVERLAPPED*>(&pMsg),
				waitTime); result == false)
			{
				return false;
			}

			switch (pMsg->Type)
			{
			case MessageType::Connection:
			{
				msgOperationType = (INT8)pMsg->Type;
				connectionIndex = pConnection->GetIndex();
			}
			break;
			case MessageType::Close:
			{
				msgOperationType = (INT8)pMsg->Type;
				connectionIndex = pConnection->GetIndex();

				pConnection->ResetConnection();
			}
			break;
			case MessageType::OnRecv:
			{
				ForwardingNetOnRecvMsgToAppLayer(pConnection, pMsg, msgOperationType, connectionIndex, pBuf, copySize, ioSize);
				m_pMsgPool->DeallocMsg(pMsg);
			}
			break;
			}

			return true;
		}*/

		BOOL PostNetMessage(NetEventType* pEvnet_)
		{
			// Boost.Asio의 경우 다른 식으로 오류에 대해 방어하고 있다. 참고하자
			//https://www.boost.org/doc/libs/1_67_0/boost/asio/detail/impl/win_iocp_io_context.ipp
			
			return PostQueuedCompletionStatus(
							mLogicIOCP,
							0,
							NULL,
							reinterpret_cast<LPOVERLAPPED>(pEvnet_));
		}
		

		
	private:		
		std::vector<SessionNetEvent> mSessionNetEventList;
		HANDLE mLogicIOCP = INVALID_HANDLE_VALUE;
	};
}