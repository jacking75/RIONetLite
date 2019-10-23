#pragma once

#include "../../C2RIONetLib/CommonDef.h"

#include <concurrent_queue.h>


enum class MessageType : INT8
{
	None = 0,
	Connection,
	Close,
	OnRecv
};

struct Message
{
	MessageType Type = MessageType::None;
	char* pContents = nullptr;

	void Clear()
	{
		Type = MessageType::None;
		pContents = nullptr;
	}

	void SetMessage(MessageType msgType, char* pSetContents)
	{
		Type = msgType;
		pContents = pSetContents;
	}
};

class MessageNetEventManager
{
public:
	MessageNetEventManager() = default;
	~MessageNetEventManager() = default;

	void OnConnect(const int index)
	{
	}

	void OnClose(const int index, const C2RIONetLib::CloseReason reason)
	{
	}

	void OnReceive()
	{
	}

	
	int Init(const unsigned int threadCount)
	{
		mLogicIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, threadCount);
		if (mLogicIOCP == INVALID_HANDLE_VALUE)
		{
			return -1;
		}

		return 0;
	}

	void Dispose()
	{
		if (mLogicIOCP != INVALID_HANDLE_VALUE){ CloseHandle(mLogicIOCP); }
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

	/*
	int PostNetMessage(Connection* pConnection, Message* pMsg, const DWORD packetSize = 0)
	{
		if (m_hLogicIOCP == INVALID_HANDLE_VALUE || pMsg == nullptr)
		{
			return NetResult::fail_message_null;
		}

		// Boost.Asio의 경우 다른 식으로 오류에 대해 방어하고 있다. 참고하자
		//https://www.boost.org/doc/libs/1_67_0/boost/asio/detail/impl/win_iocp_io_context.ipp
		auto result = PostQueuedCompletionStatus(
			m_hLogicIOCP,
			packetSize,
			reinterpret_cast<ULONG_PTR>(pConnection),
			reinterpret_cast<LPOVERLAPPED>(pMsg));

		if (!result)
		{
			char logmsg[256] = { 0, };
			sprintf_s(logmsg, "IOCPServer::PostLogicMsg - PostQueuedCompletionStatus(). error:%d, MsgType:%d", WSAGetLastError(), pMsg->Type);
			LogFuncPtr((int)LogLevel::Error, logmsg);
			return NetResult::fail_pqcs;
		}
		return NetResult::Success;
	}
	*/

	Message* AllocMsg()
	{
		Message* pMsg = nullptr;
		if (!m_MessagePool.try_pop(pMsg))
		{
			return nullptr;
		}

		return pMsg;
	}

	bool DeallocMsg(Message* pMsg)
	{
		if (pMsg == nullptr)
		{
			return false;
		}

		pMsg->Clear();

		m_MessagePool.push(pMsg);
		return true;
	}
private:
	//TODO 세션 별로 connect, close, recv(초당 최대 수 * 2)만큼 메시지 풀 가지기
	bool CreateMessagePool()
	{
		Message* pMsg = nullptr;
		for (int i = 0; i < 10; ++i)
		{
			pMsg = new Message;
			pMsg->Clear();

			m_MessagePool.push(pMsg);
		}

		return true;
	}

	void DestroyMessagePool()
	{
		while (auto pData = AllocMsg())
		{
			if (pData == nullptr)
			{
				break;
			}

			delete pData;
		}
	}


	concurrency::concurrent_queue<Message*> m_MessagePool;

	HANDLE mLogicIOCP = INVALID_HANDLE_VALUE;
};
