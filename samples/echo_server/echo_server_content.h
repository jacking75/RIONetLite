#pragma once

#include "Packet.h"

#include "../../c2_rio_net/i_net_event.h"
#include "../../c2_rio_net/server_net.h"

#include <cstdint>


class EchoServerContent : public c2rionet::INetEventManager
{
public:
	void OnConnect(const std::int32_t index, const std::uint64_t uid) override
	{
		printf("[OnConnect] - SessionIndex(%d), UniqueId(%I64u)\n", index, uid);
	}

	void OnClose(const std::int32_t index, const c2rionet::DisconnectReason reason) override
	{
		printf("[OnClose] - SessionIndex(%d), Reason(%s)\n", index, c2rionet::DisconnectReasonStr[(int)reason]);
	}

	std::uint32_t OnReceive(const std::int32_t index, const std::uint64_t uid, 
							const std::uint32_t data_size, const char* p_data) override
	{
		// 이 데이터는 방금 받은 데이터로 여기서 사용 도중 세션이 끊어져서 이 세션이 재 사용되면 버퍼의 값이 이상해질 수 있다. 이 이벤트 발생 후 RE_USE_SESSION_WAITE_TIMESEC는 보장한다
		//printf("[OnReceive] - SessionIndex(%d), size(%d)\n", index_, (int)dataSize_);

		auto usedDataSize = ProcessPacket(index, uid, data_size, p_data);
		return usedDataSize;
	}

	std::uint32_t ProcessPacket(const std::int32_t index, const std::uint64_t uid, 
								const std::uint32_t data_size, const char* p_data)
	{
		std::int32_t readPos = 0;
		auto remainSize = data_size;

		while (remainSize >= PACKET_HEADER_SIZE) {
			auto pHeader = (PacketHeader*)&p_data[readPos];

			if (pHeader->length_ > remainSize) {
				break;
			}

			pref_net_mgr_->SendData(index, uid, pHeader->length_, &p_data[readPos]);
			readPos += pHeader->length_;
			remainSize -= pHeader->length_;
		}

		return readPos;
	}

	c2rionet::ServerNetManager* pref_net_mgr_;
};