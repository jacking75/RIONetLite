#pragma once

#include "Packet.h"
#include "../../C2RIONetLib/ServerNetManager.h"

#include <iostream>

using netErrorCode = C2RIONetLib::NetErrorCode;

class EchoServerImpl
{
public:
	EchoServerImpl() = default;
	~EchoServerImpl() = default;

	bool Init(C2RIONetLib::NetConfig netConfig)
	{	
		C2RIONetLib::LogFuncPtr = [](const int eLogInfoType, const char* szLogMsg) {
			EchoServerImpl::WriteLog(eLogInfoType, szLogMsg);
		};

		EchoServerImpl::WriteToLog(netConfig);

		if (auto result = mNetManager.Initialize(netConfig, this);
			result != netErrorCode::none)
		{
			char logmsg[256] = { 0, };
			sprintf_s(logmsg, "main() - netManager.Initialize() Error: %d", result);
			WriteLog((int)C2RIONetLib::LogLevel::error, logmsg);
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

		WriteLog((int)C2RIONetLib::LogLevel::info, "End Server");
	}

	static void WriteToLog(C2RIONetLib::NetConfig netConfig)
	{
		char logmsg[256] = { 0, };

		sprintf_s(logmsg, "NetConfig - PortNumber: %d", netConfig.PortNumber);
		WriteLog((int)C2RIONetLib::LogLevel::info, logmsg);

		sprintf_s(logmsg, "NetConfig - IoWorkThreadCount: %d", netConfig.IoWorkThreadCount);
		WriteLog((int)C2RIONetLib::LogLevel::info, logmsg);

		sprintf_s(logmsg, "NetConfig - MaxConnectionCount: %d", netConfig.MaxConnectionCount);
		WriteLog((int)C2RIONetLib::LogLevel::info, logmsg);

		sprintf_s(logmsg, "NetConfig - netConfig.Conn.MaxRecvPacketSize: %d", netConfig.Conn.MaxRecvPacketSize);
		WriteLog((int)C2RIONetLib::LogLevel::info, logmsg);

		sprintf_s(logmsg, "NetConfig - netConfig.Conn.MaxTry1RecvBufferSize: %d", netConfig.Conn.MaxTry1RecvBufferSize);
		WriteLog((int)C2RIONetLib::LogLevel::info, logmsg);

		sprintf_s(logmsg, "NetConfig - Conn.MAXRecvCountPer1Sec: %d", netConfig.Conn.MAXRecvCountPer1Sec);
		WriteLog((int)C2RIONetLib::LogLevel::info, logmsg);

		sprintf_s(logmsg, "NetConfig - Conn.MaxRecvBufferSize: %d", netConfig.Conn.MaxRecvBufferSize);
		WriteLog((int)C2RIONetLib::LogLevel::info, logmsg);

		sprintf_s(logmsg, "NetConfig - Conn.MaxSendPacketSize: %d", netConfig.Conn.MaxSendPacketSize);
		WriteLog((int)C2RIONetLib::LogLevel::info, logmsg);

		sprintf_s(logmsg, "NetConfig - Conn.MAXSendCountPer1Sec: %d", netConfig.Conn.MAXSendCountPer1Sec);
		WriteLog((int)C2RIONetLib::LogLevel::info, logmsg);

		sprintf_s(logmsg, "NetConfig - Conn.MaxSendBufferSize: %d", netConfig.Conn.MaxSendBufferSize);
		WriteLog((int)C2RIONetLib::LogLevel::info, logmsg);
	}

	static void WriteLog(const int eLogInfoType, const char* szLogMsg)
	{
		printf("[%s] %s\r\n", C2RIONetLib::LogLevelStr[eLogInfoType], szLogMsg);
	}

	void OnConnect(const INT32 index_, const UINT64 uniqueId_)
	{
		printf("[OnConnect] - SessionIndex(%d), UniqueId(%I64u)\n", index_, uniqueId_);
	}

	void OnClose(const INT32 index_, const C2RIONetLib::DisconnectReason reason_)
	{
		printf("[OnClose] - SessionIndex(%d), Reason(%s)\n", index_, C2RIONetLib::DisconnectReasonStr[(int)reason_]);
	}

	UINT32 OnReceive(const INT32 index_, const UINT64 uniqueId_, 
					const UINT32 dataSize_, const char* pData_)
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
			auto pHeader = (PACKET_HEADER*)&pData_[readPos];
			if (pHeader->Length > remainSize)
			{
				break;
			}

			mNetManager.SendData(index_, uniqueId_, pHeader->Length, &pData_[readPos]);

			readPos += pHeader->Length;
			remainSize -= pHeader->Length;
		}

		return readPos;
	}


private:
	C2RIONetLib::ServerNetManager<EchoServerImpl> mNetManager;
	std::thread mLogicThread;
};
