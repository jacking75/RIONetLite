#include "Room.h"
#include "error_code.h"
#include "Packet.h"

#define MAX_USER_ID_LEN  20

void Room::Init(const std::int32_t roomNum, const std::int32_t maxUserCount)
{
	room_num_ = roomNum;
	max_client_count_ = maxUserCount;
}

std::uint16_t Room::EnterUser(ClientSession* pUser)
{
	if (current_user_count >= max_client_count_) {
		return (std::uint16_t)ErrorCode::kENTER_ROOM_FULL_USER;
	}
				
	client_list_.push_back(pUser);
	++current_user_count;

	pUser->EnterRoom(room_num_);
	return (std::uint16_t)ErrorCode::kNONE;
}


void Room::LeaveUser(ClientSession* pLeaveUser)
{
	client_list_.remove_if(
		[leaveUserId = pLeaveUser->GetUserId()](ClientSession*pUser) {
								return leaveUserId == pUser->GetUserId();
							});
}
			
void Room::NotifyChat(std::int32_t connIndex, const char* UserID, const char* Msg)
{
	ROOM_CHAT_NOTIFY_PACKET roomChatNtfyPkt;
	roomChatNtfyPkt.id_ = (std::uint16_t)PacketID::kROOM_CHAT_NOTIFY;
	roomChatNtfyPkt.length_ = sizeof(roomChatNtfyPkt);

	memcpy(roomChatNtfyPkt.Msg, Msg, sizeof(roomChatNtfyPkt.Msg));
	memcpy(roomChatNtfyPkt.UserID, UserID, sizeof(roomChatNtfyPkt.UserID));
	SendToAllUser(sizeof(roomChatNtfyPkt), &roomChatNtfyPkt, connIndex, false);
}


void Room::SendToAllUser(const std::uint16_t dataSize, void* pData, const std::int32_t passUserindex, bool exceptMe)
{

	for (auto pUser : client_list_)
	{
		if (pUser == nullptr) {
			continue;
		}

		if (exceptMe && pUser->GetNetConnIdx() == passUserindex) {
			continue;
		}
						
		SendPacketFunc(pUser->GetNetConnIdx(), pData, dataSize);
	}
}
