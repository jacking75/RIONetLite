#pragma once

enum class  PACKET_ID
{
	ECHO_REQUEST = 101,
	ECHO_RESPONSE = 102,	
};


#pragma pack(push,1)
struct PACKET_HEADER
{
	unsigned short Length;
	unsigned short Id;
	unsigned char Type; // 압축여부 암호화여부 등 속성을 알아내는 값
};

const unsigned short PACKET_HEADER_SIZE = sizeof(PACKET_HEADER);

#pragma pack(pop) //위에 설정된 패킹설정이 사라짐