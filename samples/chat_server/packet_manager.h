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

	void Init(ClientSessionManager* pUserManager, RoomManager* pRoomManager);

	void ProcessRecvPacket(std::int32_t connectionIndex, char* pBuf, std::int16_t copySize);
	void ProcessLogin(std::int32_t connIndex, char* pBuf, std::int16_t copySize);
	void ProcessEnterRoom(std::int32_t connIndex, char* pBuf, std::int16_t copySize);
	void ProcessLeaveRoom(std::int32_t connIndex, char* pBuf, std::int16_t copySize);
	void ProcessRoomChatMessage(std::int32_t connIndex, char* pBuf, std::int16_t copySize);
		

	void ClearConnectionInfo(std::int32_t connIndex);
		

	std::function<void(std::int32_t, void*, std::int16_t)> SendPacketFunc;


private:
	typedef void(PacketManager::* PROCESS_RECV_PACKET_FUNCTION)(std::int32_t, char*, std::int16_t);
	std::unordered_map<int, PROCESS_RECV_PACKET_FUNCTION> m_RecvFuntionDictionary;

	ClientSessionManager* m_pUserManager;
	RoomManager* m_pRoomManager;
		
	std::function<void(int, char*)> m_SendMQDataFunc;
};

