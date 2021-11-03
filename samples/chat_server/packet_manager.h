#pragma once

#include "client_session_manager.h"
#include "room_manager.h"

#include <unordered_map>
#include <functional>
#include <cstdint>


class PacketManager {
public:
	PacketManager() = default;
	~PacketManager() = default;

	std::function<void(std::int32_t, void*, std::int16_t)> SendPacketFunc;

	void Init(ClientSessionManager* p_session_mgr, RoomManager* p_room_mgr);

	void ProcessRecvPacket(std::int32_t client_index, std::int16_t data_size, char* p_data);
	

private:
	void ProcessLogin(std::int32_t client_index, std::int16_t data_size, char* p_data);
	void ProcessEnterRoom(std::int32_t client_index, std::int16_t data_size, char* p_data);
	void ProcessLeaveRoom(std::int32_t client_index, std::int16_t data_size, char* p_data);
	void ProcessRoomChatMessage(std::int32_t client_index, std::int16_t data_size, char* p_data);


	void ClearConnectionInfo(std::int32_t client_index);



	typedef void(PacketManager::* PROCESS_RECV_PACKET_FUNCTION)(std::int32_t, std::int16_t, char*);
	std::unordered_map<int, PROCESS_RECV_PACKET_FUNCTION> packet_func_dic_;

	ClientSessionManager* pref_client_mgr_;
	RoomManager* pref_room_mgr_;		
};

