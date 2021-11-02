#include "packet_manager.h"
#include "Packet.h"

#include <utility>
#include <cstring>


void PacketManager::Init(ClientSessionManager* pUserManager, RoomManager* pRoomManager)
{
	m_RecvFuntionDictionary = std::unordered_map<int, PROCESS_RECV_PACKET_FUNCTION>();

	m_RecvFuntionDictionary[(int)PacketID::kLOGIN_REQUEST] = &PacketManager::ProcessLogin;
	m_RecvFuntionDictionary[(int)PacketID::kROOM_ENTER_REQUEST] = &PacketManager::ProcessEnterRoom;
	m_RecvFuntionDictionary[(int)PacketID::kROOM_LEAVE_REQUEST] = &PacketManager::ProcessLeaveRoom;
	m_RecvFuntionDictionary[(int)PacketID::kROOM_CHAT_REQUEST] = &PacketManager::ProcessRoomChatMessage;
				
	m_pUserManager = pUserManager;
	m_pRoomManager = pRoomManager;
}



void PacketManager::ProcessRecvPacket(std::int32_t connectionIndex, char* pBuf, std::int16_t copySize)
{
	auto pHeader = reinterpret_cast<PacketHeader*>(pBuf);

	auto iter = m_RecvFuntionDictionary.find(pHeader->id_);
	if (iter != m_RecvFuntionDictionary.end())
	{
		(this->*(iter->second))(connectionIndex, pBuf, copySize);
	}

}
		
void PacketManager::ProcessLogin(const std::int32_t connIndex, char* pBuf, std::int16_t copySize)
{ 
	if (LOGIN_REQUEST_PACKET_SZIE != copySize)
	{
		return;
	}

	auto pLoginReqPacket = reinterpret_cast<LOGIN_REQUEST_PACKET*>(pBuf);

	auto pUserID = pLoginReqPacket->UserID;
	printf("requested user id = %s\n", pUserID);

	LOGIN_RESPONSE_PACKET loginResPacket;
	loginResPacket.PacketId = (std::uint16_t)PACKET_ID::LOGIN_RESPONSE;
	loginResPacket.PacketLength = sizeof(LOGIN_RESPONSE_PACKET);

	if (m_pUserManager->GetCurrentUserCnt() >= m_pUserManager->GetMaxUserCnt()) 
	{ 
		//접속자수가 최대수를 차지해서 접속불가
		loginResPacket.Result = (std::uint16_t)ERROR_CODE::LOGIN_USER_USED_ALL_OBJ;
		SendPacketFunc(connIndex, &loginResPacket, sizeof(LOGIN_RESPONSE_PACKET));
		return;
	}

	//여기에서 이미 접속된 유저인지 확인하고, 접속된 유저라면 실패한다.
	if (m_pUserManager->FindUserIndexByID(pUserID) == -1) 
	{ 
		//접속중이 아닌 유저라면
		//유저를 관리하는 객체에 넣는다.
		m_pUserManager->AddUser(pUserID, connIndex);
		loginResPacket.Result = (std::uint16_t)ERROR_CODE::NONE;
	}
	else 
	{
		//접속중인 유저여서 실패를 반환한다.
		loginResPacket.Result = (std::uint16_t)ERROR_CODE::LOGIN_USER_ALREADY;
		SendPacketFunc(connIndex, &loginResPacket, sizeof(LOGIN_RESPONSE_PACKET));
		return;
	}

	SendPacketFunc(connIndex, &loginResPacket, sizeof(LOGIN_RESPONSE_PACKET));				
}



void PacketManager::ProcessEnterRoom(std::int32_t connIndex, char* pBuf, std::int16_t copySize)
{
	UNREFERENCED_PARAMETER(copySize);

	auto pRoomEnterReqPacket = reinterpret_cast<ROOM_ENTER_REQUEST_PACKET*>(pBuf);
	auto pReqUser = m_pUserManager->GetUserByConnIdx(connIndex);

	if (!pReqUser || pReqUser == nullptr) 
	{
		return;
	}
				
	ROOM_ENTER_RESPONSE_PACKET roomEnterResPacket;
	roomEnterResPacket.PacketId = (std::uint16_t)PACKET_ID::ROOM_ENTER_RESPONSE;
	roomEnterResPacket.PacketLength = sizeof(ROOM_ENTER_RESPONSE_PACKET);
				
	roomEnterResPacket.Result = m_pRoomManager->EnterUser(pRoomEnterReqPacket->RoomNumber, pReqUser);

	SendPacketFunc(connIndex, &roomEnterResPacket, sizeof(ROOM_ENTER_RESPONSE_PACKET));
	printf("Response Packet Sended");
}


void PacketManager::ProcessLeaveRoom(std::int32_t connIndex, char* pBuf, std::int16_t copySize)
{
	UNREFERENCED_PARAMETER(pBuf);
	UNREFERENCED_PARAMETER(copySize);

	ROOM_LEAVE_RESPONSE_PACKET roomLeaveResPacket;
	roomLeaveResPacket.PacketId = (std::uint16_t)PACKET_ID::ROOM_LEAVE_RESPONSE;
	roomLeaveResPacket.PacketLength = sizeof(ROOM_LEAVE_RESPONSE_PACKET);

	auto reqUser = m_pUserManager->GetUserByConnIdx(connIndex);
	auto roomNum = reqUser->GetCurrentRoom();
				
	//TODO Room안의 UserList객체의 값 확인하기
	roomLeaveResPacket.Result = m_pRoomManager->LeaveUser(roomNum, reqUser);
	SendPacketFunc(connIndex, &roomLeaveResPacket, sizeof(ROOM_LEAVE_RESPONSE_PACKET));
}


void PacketManager::ProcessRoomChatMessage(std::int32_t connIndex, char* pBuf, std::int16_t copySize)
{
	UNREFERENCED_PARAMETER(copySize);

	auto pRoomChatReqPacketet = reinterpret_cast<ROOM_CHAT_REQUEST_PACKET*>(pBuf);
		
	ROOM_CHAT_RESPONSE_PACKET roomChatResPacket;
	roomChatResPacket.PacketId = (std::uint16_t)PACKET_ID::ROOM_CHAT_RESPONSE;
	roomChatResPacket.PacketLength = sizeof(ROOM_CHAT_RESPONSE_PACKET);
	roomChatResPacket.Result = (std::uint16_t)ERROR_CODE::NONE;

	auto reqUser = m_pUserManager->GetUserByConnIdx(connIndex);
	auto roomNum = reqUser->GetCurrentRoom();
				
	auto pRoom = m_pRoomManager->GetRoomByNumber(roomNum);
	if (pRoom == nullptr)
	{
		roomChatResPacket.Result = (std::uint16_t)ERROR_CODE::CHAT_ROOM_INVALID_ROOM_NUMBER;
		SendPacketFunc(connIndex, &roomChatResPacket, sizeof(ROOM_CHAT_RESPONSE_PACKET));
		return;
	}
		
	SendPacketFunc(connIndex, &roomChatResPacket, sizeof(ROOM_CHAT_RESPONSE_PACKET));

	pRoom->NotifyChat(connIndex, reqUser->GetUserId().c_str(), pRoomChatReqPacketet->Message);		
}		   

void PacketManager::ClearConnectionInfo(std::int32_t connIndex)
{
	auto pReqUser = m_pUserManager->GetUserByConnIdx(connIndex);

	if (pReqUser->GetDomainState() == User::DOMAIN_STATE::ROOM) 
	{
		auto roomNum = pReqUser->GetCurrentRoom();
		m_pRoomManager->LeaveUser(roomNum, pReqUser);
	}

	if (pReqUser->GetDomainState() != User::DOMAIN_STATE::NONE) 
	{
		m_pUserManager->DeleteUserInfo(pReqUser);
	}
}