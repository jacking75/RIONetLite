#pragma once

#include "net_event_msg.h"
#include "net_event_msg_pool.h"

#include "../../c2_rio_net/i_net_event.h"
#include "../../c2_rio_net/server_net.h"

#include <cstdint>
#include <span>


class ChatServerContent : public c2rionet::INetEventManager
{
public:
	ChatServerContent() = default;
	~ChatServerContent() = default;

	bool Init(c2rionet::ServerNetManager* p_net_mgr);

	void OnConnect(const std::int32_t index, const std::uint64_t uid) override;
	
	void OnClose(const std::int32_t index, const c2rionet::DisconnectReason reason) override;

	std::uint32_t OnReceive(const std::int32_t index, const std::uint64_t uid,
					const std::uint32_t data_size, const char* p_data) override;
	
	
private:
	/*void Dispose()
	{
		if (mLogicIOCP != INVALID_HANDLE_VALUE) { CloseHandle(mLogicIOCP); }
	}*/

	BOOL PostNetMessage(NetEventType* pEvnet_);

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



	c2rionet::ServerNetManager* pref_net_mgr_;

	NetEventMsgObjectPool net_event_msg_pool_;

	HANDLE mLogicIOCP = INVALID_HANDLE_VALUE;
};