#include "packet_manager.h"
#include "Packet.h"
#include "logger.h"

#include <format>
#include <utility>
#include <cstring>


void PacketManager::Init(ClientSessionManager* p_client_mgr, RoomManager* p_room_mgr)
{
	packet_func_dic_ = std::unordered_map<int, PROCESS_RECV_PACKET_FUNCTION>();

	packet_func_dic_[(int)PacketID::kLOGIN_REQUEST] = &PacketManager::ProcessLogin;
	packet_func_dic_[(int)PacketID::kROOM_ENTER_REQUEST] = &PacketManager::ProcessEnterRoom;
	packet_func_dic_[(int)PacketID::kROOM_LEAVE_REQUEST] = &PacketManager::ProcessLeaveRoom;
	packet_func_dic_[(int)PacketID::kROOM_CHAT_REQUEST] = &PacketManager::ProcessRoomChatMessage;
				
	pref_client_mgr_ = p_client_mgr;
	pref_room_mgr_ = p_room_mgr;
}



void PacketManager::ProcessRecvPacket(std::int32_t client_index, const std::uint64_t client_uid, std::int16_t data_size, char* p_data)
{
	auto p_header = reinterpret_cast<PacketHeader*>(p_data);

	auto iter = packet_func_dic_.find(p_header->id_);
	if (iter != packet_func_dic_.end()) {
		(this->*(iter->second))(client_index, client_uid, data_size, p_data);
	}

}
		
void PacketManager::ProcessLogin(const std::int32_t client_index, const std::uint64_t client_uid, std::int16_t data_size, char* p_data)
{ 
	if (LOGIN_REQUEST_PACKET_SZIE != data_size) {
		return;
	}

	auto p_login_req_packet = reinterpret_cast<LoginRequestPacket*>(p_data);
	auto p_cleint_id = p_login_req_packet->UserID;

	Logger::Debug(std::format("requested user id = {0}", p_cleint_id).c_str());

	LoginResponsePacket loginResPacket;
	loginResPacket.id_ = (std::uint16_t)PacketID::kLOGIN_RESPONSE;
	loginResPacket.length_ = sizeof(LoginResponsePacket);

	if (pref_client_mgr_->CurrentClientCount() >= pref_client_mgr_->MaxClientCount()) { 
		//접속자수가 최대수를 차지해서 접속불가
		loginResPacket.Result = (std::uint16_t)ErrorCode::kLOGIN_USER_USED_ALL_OBJ;
		SendPacketFunc(client_index, client_uid, sizeof(LoginResponsePacket), (char*)&loginResPacket);
		return;
	}

	//여기에서 이미 접속된 유저인지 확인하고, 접속된 유저라면 실패한다.
	if (pref_client_mgr_->FindClientIndexByID(p_cleint_id) == -1) { 
		//접속중이 아닌 유저라면
		//유저를 관리하는 객체에 넣는다.
		pref_client_mgr_->AddClient(p_cleint_id, client_index);
		loginResPacket.Result = (std::uint16_t)ErrorCode::kNONE;
	} else {
		//접속중인 유저여서 실패를 반환한다.
		loginResPacket.Result = (std::uint16_t)ErrorCode::kLOGIN_USER_ALREADY;
		SendPacketFunc(client_index, client_uid, sizeof(LoginResponsePacket), (char*)&loginResPacket);
		return;
	}

	SendPacketFunc(client_index, client_uid, sizeof(LoginResponsePacket), (char*)&loginResPacket);
}



void PacketManager::ProcessEnterRoom(std::int32_t client_index, const std::uint64_t client_uid, [[maybe_unused]] std::int16_t data_size, char* p_data)
{
	auto pRoomEnterReqPacket = reinterpret_cast<RoomEnterRequestPacket*>(p_data);
	auto p_req_client = pref_client_mgr_->GetClientByConnIdx(client_index);

	if (!p_req_client || p_req_client == nullptr) {
		return;
	}
				
	RoomEnterResponsePacket room_enter_res_packet;
	room_enter_res_packet.id_ = (std::uint16_t)PacketID::kROOM_ENTER_RESPONSE;
	room_enter_res_packet.length_ = sizeof(RoomEnterResponsePacket);
				
	room_enter_res_packet.Result = pref_room_mgr_->EnterClient(pRoomEnterReqPacket->RoomNumber, p_req_client);

	SendPacketFunc(client_index, client_uid, sizeof(RoomEnterResponsePacket) , (char*)&room_enter_res_packet );
	printf("Response Packet Sended");
}


void PacketManager::ProcessLeaveRoom(std::int32_t client_index, const std::uint64_t client_uid, [[maybe_unused]] std::int16_t data_size, [[maybe_unused]] char* p_data)
{
	RoomLeaveResponsePacket roomLeaveResPacket;
	roomLeaveResPacket.id_ = (std::uint16_t)PacketID::kROOM_LEAVE_RESPONSE;
	roomLeaveResPacket.length_ = sizeof(RoomLeaveResponsePacket);

	auto req_client = pref_client_mgr_->GetClientByConnIdx(client_index);
	auto room_num = req_client->GetCurrentRoom();
				
	//TODO Room안의 UserList객체의 값 확인하기
	roomLeaveResPacket.Result = pref_room_mgr_->LeaveClient(room_num, req_client);
	SendPacketFunc(client_index, client_uid, sizeof(RoomLeaveResponsePacket), (char*)&roomLeaveResPacket);
}


void PacketManager::ProcessRoomChatMessage(std::int32_t client_index, const std::uint64_t client_uid, [[maybe_unused]] std::int16_t data_size, char* p_data)
{
	auto p_room_chat_req_packet = reinterpret_cast<RoomChatRequestPacket*>(p_data);
		
	RoomChatResponsePacket roomChatResPacket;
	roomChatResPacket.id_ = (std::uint16_t)PacketID::kROOM_CHAT_RESPONSE;
	roomChatResPacket.length_ = sizeof(RoomChatResponsePacket);
	roomChatResPacket.Result = (std::uint16_t)ErrorCode::kNONE;


	auto reqUser = pref_client_mgr_->GetClientByConnIdx(client_index);
	auto roomNum = reqUser->GetCurrentRoom();
				
	auto pRoom = pref_room_mgr_->GetRoomByNumber(roomNum);
	if (pRoom == nullptr) {
		roomChatResPacket.Result = (std::uint16_t)ErrorCode::kCHAT_ROOM_INVALID_ROOM_NUMBER;
		SendPacketFunc(client_index, client_uid, sizeof(RoomChatResponsePacket) , (char*)&roomChatResPacket );
		return;
	}
		
	SendPacketFunc(client_index, client_uid, sizeof(RoomChatResponsePacket), (char*)&roomChatResPacket);

	pRoom->NotifyChat(client_index, reqUser->GetUserId().c_str(), p_room_chat_req_packet->Message);		
}		   

void PacketManager::ClearConnectionInfo(std::int32_t client_index)
{
	auto p_req_client = pref_client_mgr_->GetClientByConnIdx(client_index);

	if (p_req_client->GetDomainState() == ClientSession::DomainState::kROOM) {
		auto roomNum = p_req_client->GetCurrentRoom();
		pref_room_mgr_->LeaveClient(roomNum, p_req_client);
	}

	if (p_req_client->GetDomainState() != ClientSession::DomainState::kNONE) {
		pref_client_mgr_->RemoveClient(p_req_client);
	}
}