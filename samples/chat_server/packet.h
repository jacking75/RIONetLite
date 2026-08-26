#pragma once

#include <cstdint>

enum class PacketID {
  kECHO = 101,

  kLOGIN_REQUEST = 201,
  kLOGIN_RESPONSE = 202,

  kROOM_ENTER_REQUEST = 206,
  kROOM_ENTER_RESPONSE = 207,
  PACKET_ID_ROOM_USER_LIST_NTF = 208,
  PACKET_ID_ROOM_NEW_USER_NTF = 219,

  kROOM_LEAVE_REQUEST = 216,
  kROOM_LEAVE_RESPONSE = 217,
  kROOM_LEAVE_NOTIFY = 218,

  kROOM_CHAT_REQUEST = 221,
  kROOM_CHAT_RESPONSE = 222,
  kROOM_CHAT_NOTIFY = 223,
};

#pragma pack(push, 1)
struct PacketHeader {
  unsigned short length_;
  unsigned short id_;
  unsigned char type_;  // 압축여부 암호화여부 등 속성을 알아내는 값
};

const unsigned short PACKET_HEADER_SIZE = sizeof(PacketHeader);

//- 로그인 요청
const int MAX_USER_ID_LEN = 20;
const int MAX_USER_PW_LEN = 20;

struct LoginRequestPacket : public PacketHeader {
  char UserID[MAX_USER_ID_LEN + 1];
  char UserPW[MAX_USER_PW_LEN + 1];
};
const size_t LOGIN_REQUEST_PACKET_SZIE = sizeof(LoginRequestPacket);

struct LoginResponsePacket : public PacketHeader {
  std::uint16_t Result;
};

//- 룸에 들어가기 요청
const int MAX_ROOM_TITLE_SIZE = 16;
struct RoomEnterRequestPacket : public PacketHeader {
  int RoomNumber;
};

struct RoomEnterResponsePacket : public PacketHeader {
  std::uint16_t Result;
};

struct RoomUserListNotifyPacket : public PacketHeader {
  char user_count_;
  char data_[1];
};

struct RoomNewUserNotifyPacket : public PacketHeader {
  char data_[1];
};

//- 룸 나가기 요청
struct RoomLeaveRequestPacket : public PacketHeader {};

struct RoomLeaveResponsePacket : public PacketHeader {
  std::uint16_t Result;
};

struct RoomLeaveNotifyPacket : public PacketHeader {
  char UserID[MAX_USER_ID_LEN + 1] = {
      0,
  };
};

// 룸 채팅
const int MAX_CHAT_MSG_SIZE = 256;
struct RoomChatRequestPacket : public PacketHeader {
  char Message[MAX_CHAT_MSG_SIZE + 1] = {
      0,
  };
};

struct RoomChatResponsePacket : public PacketHeader {
  std::uint16_t Result;
};

struct RoomChatNotifyPacket : public PacketHeader {
  char UserID[MAX_USER_ID_LEN + 1] = {
      0,
  };
  char Msg[MAX_CHAT_MSG_SIZE + 1] = {
      0,
  };
};

#pragma pack(pop)  // 위에 설정된 패킹설정이 사라짐