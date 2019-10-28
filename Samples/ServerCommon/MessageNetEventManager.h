#pragma once

#include "../../C2RIONetLib/CommonDef.h"

#include <vector>
#include <concurrent_queue.h>


namespace ServerCommon
{
	enum class NetEventType : INT8
	{
		None = 0,
		Connection,
		Close,
		OnRecv
	};

	struct NetEvent
	{		
		int SessionIndex = -1;
		NetEventType EventType = NetEventType::None;
		unsigned int DataSize = 0;
		char* pData = nullptr;
		
		void Set(const int sessionIndex, const NetEventType type, const unsigned int dataSize, char* data)
		{
			SessionIndex = sessionIndex;
			EventType = type;
			DataSize = dataSize;
			pData = data;
		}

		void SetReceive(const unsigned int dataSize, char* data)
		{
			DataSize = dataSize;
			pData = data;
		}		
	};

	class NetEventRecvRingBuffer
	{
	public:
		NetEventRecvRingBuffer() = default;
		~NetEventRecvRingBuffer() = default;

		void Init(const int sessionIndex, const unsigned int maxCount)
		{
			mMaxCount = maxCount;

			for (int i = 0; i < maxCount; ++i)
			{
				Objects.emplace_back({ sessionIndex,  NetEventType::OnRecv, 0, nullptr });
			}
		}

		void Clear()
		{
			mCurPos = 0;
		}		

		NetEvent* GetData()
		{
			if (mCurPos >= mMaxCount) { mMaxCount = 0; }

			return &Objects[mCurPos];
		}

	private:
		std::vector<NetEvent> Objects;
		unsigned int mMaxCount;
		unsigned int mCurPos;
	};

	class SessionNetEvent
	{
	public:
		SessionNetEvent() = default;
		~SessionNetEvent() = default;

		void Init(const int sessionIndex, const unsigned int maxRecvEvnetCount)
		{
			mConnect.Set(sessionIndex, NetEventType::Connection, 0, nullptr);
			mConnect.Set(sessionIndex, NetEventType::Close, 0, nullptr);
			mRecvs.Init(sessionIndex, maxRecvEvnetCount);
		}

		NetEvent* GetConnect() { return &mConnect; }
		NetEvent* GetClose() { return &mClose; }
		NetEvent* GetRecv() { return mRecvs.GetData(); }
		
	private:
		NetEvent mConnect;
		NetEvent mClose;
		NetEventRecvRingBuffer mRecvs;
	};

	class MessageNetEventManager
	{
	public:
		MessageNetEventManager() = default;
		~MessageNetEventManager() = default;

		void OnConnect(const int index)
		{
		}

		void OnClose(const int index, const C2RIONetLib::DisconnectReason reason)
		{
		}

		void OnReceive()
		{
		}


		int Init(const unsigned int threadCount, const unsigned int sessionCount, const unsigned int maxRecvEvnetCount)
		{
			mLogicIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, threadCount);
			if (mLogicIOCP == INVALID_HANDLE_VALUE)
			{
				return -1;
			}

			for (auto i = decltype(sessionCount); i < sessionCount; ++i)
			{
				SessionNetEvent session;
				session.Init(i, maxRecvEvnetCount);
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

		void PostRecvEventMsg(const int dataSize, const char* pData)
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

		BOOL PostNetMessage(NetEventType* pEvnet)
		{
			// Boost.Asio의 경우 다른 식으로 오류에 대해 방어하고 있다. 참고하자
			//https://www.boost.org/doc/libs/1_67_0/boost/asio/detail/impl/win_iocp_io_context.ipp
			
			return PostQueuedCompletionStatus(
							mLogicIOCP,
							0,
							NULL,
							reinterpret_cast<LPOVERLAPPED>(pEvnet));
		}
		

		
	private:		
		std::vector<SessionNetEvent> mSessionNetEventList;

		HANDLE mLogicIOCP = INVALID_HANDLE_VALUE;
	};
}