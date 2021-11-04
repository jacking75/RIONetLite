#include "Room.h"
#include "error_code.h"
#include "Packet.h"

#define MAX_USER_ID_LEN  20

void Room::Init(const std::int32_t room_num, const std::int32_t max_client_count)
{
	room_num_ = room_num;
	max_client_count_ = max_client_count;
}

std::uint16_t Room::EnterClient(ClientSession* p_client)
{
	if (current_client_count_ >= max_client_count_) {
		return (std::uint16_t)ErrorCode::kENTER_ROOM_FULL_USER;
	}
				
	client_list_.push_back(p_client);
	++current_client_count_;

	p_client->EnterRoom(room_num_);
	return (std::uint16_t)ErrorCode::kNONE;
}


void Room::LeaveClient(ClientSession* p_leave_client)
{
	client_list_.remove_if(
		[leaveUserId = p_leave_client->GetUserId()](ClientSession*pUser) {
								return leaveUserId == pUser->GetUserId();
							});
}
			
void Room::NotifyChat(std::int32_t client_index, const char* p_user_id, const char* p_msg)
{
	RoomChatNotifyPacket roomChatNtfyPkt;
	roomChatNtfyPkt.id_ = (std::uint16_t)PacketID::kROOM_CHAT_NOTIFY;
	roomChatNtfyPkt.length_ = sizeof(roomChatNtfyPkt);

	memcpy(roomChatNtfyPkt.Msg, p_msg, sizeof(roomChatNtfyPkt.Msg));
	memcpy(roomChatNtfyPkt.UserID, p_user_id, sizeof(roomChatNtfyPkt.UserID));
	SendToAllUser(sizeof(roomChatNtfyPkt), (char*)&roomChatNtfyPkt, client_index, false);
}


void Room::SendToAllUser(const std::uint16_t data_size, char* p_data, const std::int32_t pass_client_index, bool is_except_me)
{

	for (auto p_session : client_list_)
	{
		if (p_session == nullptr) {
			continue;
		}

		if (is_except_me && p_session->SessionIndex() == pass_client_index) {
			continue;
		}
						
		SendPacketFunc(p_session->SessionIndex(), p_session->SessionUID(), data_size, p_data);
	}
}
