#pragma once

#include "Packet.h"
#include "../../c2_rio_net/i_net_event_manager.h"
#include "../../c2_rio_net/server_net_manager.h"

#include <iostream>

using netErrorCode = c2rionet::NetErrorCode;

class EchoServerNetEventManager : public c2rionet::INetEventManager
{
public:
	void OnConnect(const INT32 index_, const UINT64 uniqueId_) override 
	{
		printf("[OnConnect] - SessionIndex(%d), UniqueId(%I64u)\n", index_, uniqueId_);
	}

	void OnClose(const INT32 index_, const c2rionet::DisconnectReason reason_) override 
	{
		printf("[OnClose] - SessionIndex(%d), Reason(%s)\n", index_, c2rionet::DisconnectReasonStr[(int)reason_]);
	}

	UINT32 OnReceive(const INT32 index_, const UINT64 uniqueId_, const UINT32 dataSize_, const char* pData_) override 
	{
		// 이 데이터는 방금 받은 데이터로 여기서 사용 도중 세션이 끊어져서 이 세션이 재 사용되면 버퍼의 값이 이상해질 수 있다. 이 이벤트 발생 후 RE_USE_SESSION_WAITE_TIMESEC는 보장한다
		//printf("[OnReceive] - SessionIndex(%d), size(%d)\n", index_, (int)dataSize_);

		auto usedDataSize = ProcessPacket(index_, uniqueId_, dataSize_, pData_);
		return usedDataSize;
	}

	UINT32 ProcessPacket(const INT32 index_, const UINT64 uniqueId_,
		const UINT32 dataSize_, const char* pData_)
	{
		INT32 readPos = 0;
		auto remainSize = dataSize_;

		while (remainSize >= PACKET_HEADER_SIZE)
		{
			auto pHeader = (PacketHeader*)&pData_[readPos];
			if (pHeader->length_ > remainSize)
			{
				break;
			}

			mNetManager->SendData(index_, uniqueId_, pHeader->length_, &pData_[readPos]);

			readPos += pHeader->length_;
			remainSize -= pHeader->length_;
		}

		return readPos;
	}

	c2rionet::ServerNetManager* mNetManager;
};

class EchoServer
{
public:
	EchoServer() = default;
	~EchoServer() = default;

	bool Init(c2rionet::NetConfig netConfig)
	{	
		c2rionet::LogFuncPtr = [](const int eLogInfoType, const char* szLogMsg) {
			EchoServer::WriteLog(eLogInfoType, szLogMsg);
		};

		EchoServer::WriteToLog(netConfig);

		mNetEvent.mNetManager = &mNetManager;
		if (auto result = mNetManager.Initialize(netConfig, &mNetEvent);
			result != netErrorCode::kNONE)
		{
			char logmsg[256] = { 0, };
			sprintf_s(logmsg, "main() - netManager.Initialize() Error: %d", result);
			WriteLog((int)c2rionet::LogLevel::kERROR, logmsg);
			return false;
		}

		return true;
	}

	void Start()
	{
		mLogicThread = std::thread([&]() { mNetManager.StartAndBlocking(); });
		std::this_thread::sleep_for(std::chrono::milliseconds(256));
	}

	void End()
	{
		mNetManager.End();

		if (mLogicThread.joinable())
		{
			mLogicThread.join();
		}

		WriteLog((int)c2rionet::LogLevel::kINFO, "End Server");
	}

	static void WriteToLog(c2rionet::NetConfig netConfig)
	{
		char logmsg[256] = { 0, };

		sprintf_s(logmsg, "NetConfig - PortNumber: %d", netConfig.port_number_);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);

		sprintf_s(logmsg, "NetConfig - IoWorkThreadCount: %d", netConfig.io_work_thread_count_);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);

		sprintf_s(logmsg, "NetConfig - MaxConnectionCount: %d", netConfig.max_connection_count_);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);

		sprintf_s(logmsg, "NetConfig - netConfig.Conn.MaxRecvPacketSize: %d", netConfig.conn_cfg_.max_recv_packet_size_);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);

		sprintf_s(logmsg, "NetConfig - netConfig.Conn.MaxTry1RecvBufferSize: %d", netConfig.conn_cfg_.max_try_1recv_buffer_size);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);

		sprintf_s(logmsg, "NetConfig - Conn.MAXRecvCountPer1Sec: %d", netConfig.conn_cfg_.max_recv_count_per_1sec_);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);

		sprintf_s(logmsg, "NetConfig - Conn.MaxRecvBufferSize: %d", netConfig.conn_cfg_.max_recv_buffer_size_);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);

		sprintf_s(logmsg, "NetConfig - Conn.MaxSendPacketSize: %d", netConfig.conn_cfg_.max_send_packet_size_);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);

		sprintf_s(logmsg, "NetConfig - Conn.MAXSendCountPer1Sec: %d", netConfig.conn_cfg_.max_send_count_per_1sec_);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);

		sprintf_s(logmsg, "NetConfig - Conn.MaxSendBufferSize: %d", netConfig.conn_cfg_.max_send_buffer_size_);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);
	}

	static void WriteLog(const int eLogInfoType, const char* szLogMsg)
	{
		printf("[%s] %s\r\n", c2rionet::LogLevelStr[eLogInfoType], szLogMsg);
	}

	


private:
	EchoServerNetEventManager mNetEvent;
	c2rionet::ServerNetManager mNetManager;
	std::thread mLogicThread;
};
