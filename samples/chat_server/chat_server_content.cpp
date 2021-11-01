#include "chat_server_content.h"
#include "chat_server.h"
#include "logger.h"

#include "Packet.h"

#include <format>


bool ChatServerContent::Init(c2rionet::ServerNetManager* p_net_mgr)
{
	pref_net_mgr_ = p_net_mgr;

	//TODO 설정 정보에서 가져오기
	const int max_session_count = 12;
	const int max_msg_count_per_session = 8;

	net_event_msg_pool_.Init(max_session_count, max_msg_count_per_session);


	msg_queue_iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
	if (msg_queue_iocp_ == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	return true;
}

void ChatServerContent::OnConnect(const std::int32_t index, const std::uint64_t uid)
{
	Logger::WriteLog((int)c2rionet::LogLevel::kINFO,
		std::format("[OnConnect] - SessionIndex({0}), UniqueId({1})", index, uid).c_str());
	
	auto p_event_msg = net_event_msg_pool_.Alloc(index);
	p_event_msg->session_index_ = index;
	p_event_msg->event_type_ = NetEventType::kCONNECTION;
	
	PostNetMessage(p_event_msg);
}

void ChatServerContent::OnClose(const std::int32_t index, const c2rionet::DisconnectReason reason) 
{
	printf("[OnClose] - SessionIndex(%d), Reason(%s)\n", index, c2rionet::DisconnectReasonStr[(int)reason]);
}

std::uint32_t ChatServerContent::OnReceive(const std::int32_t index, const std::uint64_t uid,
	const std::uint32_t data_size, const char* p_data) 
{
	// 이 데이터는 방금 받은 데이터로 여기서 사용 도중 세션이 끊어져서 이 세션이 재 사용되면 버퍼의 값이 이상해질 수 있다. 이 이벤트 발생 후 RE_USE_SESSION_WAITE_TIMESEC는 보장한다
	//printf("[OnReceive] - SessionIndex(%d), size(%d)\n", index_, (int)dataSize_);
	//auto usedDataSize = ProcessPacket(index, uid, data_size, p_data);
	//return usedDataSize;

	const int MAX_PACKET_BODY_SIZE = 1024; //TODO config의 값을 사용하도록 변경 필요
	auto readPos = 0;
	PacketHeader* pPktHeader;

	while ((data_size - readPos) >= PACKET_HEADER_SIZE)
	{
		pPktHeader = (PacketHeader*)p_data[readPos];
		readPos += PACKET_HEADER_SIZE;
		auto bodySize = (int16_t)(pPktHeader->length_ - PACKET_HEADER_SIZE);

		if (bodySize > 0)
		{
			if (bodySize > (data_size - readPos))
			{
				readPos -= PACKET_HEADER_SIZE;
				break;
			}

			if (bodySize > MAX_PACKET_BODY_SIZE)
			{
				// 더 이상 이 세션과는 작업을 하지 않을 예정. 클라이언트 보고 나가라고 하던가 직접 짤라야 한다.
				return readPos;
			}
		}

		//AddPacketQueue(sessionIndex, pPktHeader->Id, bodySize, &session.pRecvBuffer[readPos]);
		readPos += bodySize;
	}

	//session.RemainingDataSize -= readPos;
	//session.PrevReadPosInRecvBuffer = readPos;

	return 0;
}

BOOL ChatServerContent::PostNetMessage(NetEventMsg* pEvnet_)
{
	// 오류는 절대 없다고 가정한다.
	// Boost.Asio의 경우 다른 식으로 오류에 대해 방어하고 있다. 참고하자
	//https://www.boost.org/doc/libs/1_67_0/boost/asio/detail/impl/win_iocp_io_context.ipp

	return PostQueuedCompletionStatus(msg_queue_iocp_,
										0,
										NULL,
										reinterpret_cast<LPOVERLAPPED>(pEvnet_));
}

// std::span
//https://docs.google.com/presentation/d/1bC_V1_wQsRL-N7igEc_JH_pWqfXMqmZkUMEYF9WKwZ0/edit?usp=sharing