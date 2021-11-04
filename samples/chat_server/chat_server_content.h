#pragma once

#include "net_event_msg.h"
#include "net_event_msg_pool.h"
#include "client_session_manager.h"
#include "room_manager.h"
#include "packet_manager.h"
#include "error_code.h"

#include "../../c2_rio_net/i_net_event.h"
#include "../../c2_rio_net/server_net.h"

#include <thread>
#include <cstdint>


class ChatServerContent : public c2rionet::INetEventManager
{
public:
	ChatServerContent() = default;
	~ChatServerContent() = default;

	bool Init(c2rionet::ServerNetManager* p_net_mgr);

	void Start();

	void Stop();


	void OnConnect(const std::int32_t index, const std::uint64_t uid) override;
	
	void OnClose(const std::int32_t index, const std::uint64_t uid, const c2rionet::DisconnectReason reason) override;

	std::uint32_t OnReceive(const std::int32_t index, const std::uint64_t uid,
					const std::uint32_t data_size, char* p_data) override;
	
		
private:
	void Update();

	void SetSendFunction();

	bool InitNetMsgQueue()
	{
		msg_queue_iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
		if (msg_queue_iocp_ == INVALID_HANDLE_VALUE) {
			return false;
		}
		return true;
	}

	std::tuple<NetEventMsg*, ErrorCode> GenerateNetEventMsg(const int32_t session_index,
		const int packet_data_size, char* p_packet_data,
		const int max_packet_size);

	BOOL PostNetMessage(NetEventMsg* pEvnet_);

	


	c2rionet::ServerNetManager* pref_net_mgr_ = nullptr;

	NetEventMsgObjectPool net_event_msg_pool_;

	ClientSessionManager client_session_mgr_;
	RoomManager room_mgr_;
	PacketManager packet_mgr_;

	bool is_run_ = false;
	HANDLE msg_queue_iocp_ = INVALID_HANDLE_VALUE;
	std::thread work_thread_;
};