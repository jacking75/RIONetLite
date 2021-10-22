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

#pragma pack(pop) //위에 설정된 패킹설정이 사라짐