#pragma once

enum class PacketId
{
	kECHO_REQUEST = 101,
	kECHO_RESPONSE = 102,	
};


#pragma pack(push,1)
struct PacketHeader
{
	unsigned short length_;
	unsigned short id_;
	unsigned char type_; // 압축여부 암호화여부 등 속성을 알아내는 값
};

const unsigned short PACKET_HEADER_SIZE = sizeof(PacketHeader);


//- 로그인 요청
const int MAX_USER_ID_LEN = 20;
const int MAX_USER_PW_LEN = 20;

struct LOGIN_REQUEST_PACKET : public PacketHeader
{
	char UserID[MAX_USER_ID_LEN + 1];
	char UserPW[MAX_USER_PW_LEN + 1];
};
const size_t LOGIN_REQUEST_PACKET_SZIE = sizeof(LOGIN_REQUEST_PACKET);


struct LOGIN_RESPONSE_PACKET : public PacketHeader
{
	UINT16 Result;
};



//- 룸에 들어가기 요청
const int MAX_ROOM_TITLE_SIZE = 16;
struct ROOM_ENTER_REQUEST_PACKET : public PacketHeader
{
	INT32 RoomNumber;
};

struct ROOM_ENTER_RESPONSE_PACKET : public PacketHeader
{
	INT16 Result;
	char RivalUserID[MAX_USER_ID_LEN + 1] = { 0, };
};


//- 룸 나가기 요청
struct ROOM_LEAVE_REQUEST_PACKET : public PacketHeader
{

};

struct ROOM_LEAVE_RESPONSE_PACKET : public PacketHeader
{
	INT16 Result;
};



// 룸 채팅
const int MAX_CHAT_MSG_SIZE = 256;
struct ROOM_CHAT_REQUEST_PACKET : public PacketHeader
{
	char Message[MAX_CHAT_MSG_SIZE + 1] = { 0, };
};

struct ROOM_CHAT_RESPONSE_PACKET : public PacketHeader
{
	INT16 Result;
};

struct ROOM_CHAT_NOTIFY_PACKET : public PacketHeader
{
	char UserID[MAX_USER_ID_LEN + 1] = { 0, };
	char Msg[MAX_CHAT_MSG_SIZE + 1] = { 0, };
};


#pragma pack(pop) //위에 설정된 패킹설정이 사라짐