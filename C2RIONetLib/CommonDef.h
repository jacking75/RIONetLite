#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace C2RIONetLib
{
	struct NetConfig
	{
		UINT16 PortNumber = 0;
		INT32 WorkThreadCount = 0;
		INT32 MaxRecvOverlappedBufferSize = 0; // 한번에 받을 수 있는 최대 크기
		INT32 MaxSendOverlappedBufferSize = 0; // 한번에 보낼 수 있는 최대 크기
		INT32 ConnectionMaxRecvBufferSize = 0; // 받기용 버퍼의 최대 크기(데이터를 받으면 여기에 저장되고, 데이터의 위치가 애플리케이션에 전달되므로 넉넉하게 커야 한다)
		INT32 ConnectionMaxSendBufferSize = 0; // 보내기용 버퍼의 최대 크기
		INT32 MaxPacketSize = 0;
		INT32 MaxConnectionCount = 0;
		INT32 MaxMessagePoolCount = 0;
		INT32 ExtraMessagePoolCount = 0;
		INT32 PerformancePacketMillisecondsTime = 0;
	};

	struct ConnectionNetConfig
	{
		INT32 MaxRecvBufferSize;
		INT32 MaxSendBufferSize;
		INT32 MaxRecvOverlappedBufferSize;
		INT32 MaxSendOverlappedBufferSize;
	};


	enum class MessageType : INT8
	{
		None = 0,
		Connection,
		Close,
		OnRecv
	};

	struct Message
	{
		MessageType Type = MessageType::None;
		char* pContents = nullptr;

		void Clear()
		{
			Type = MessageType::None;
			pContents = nullptr;
		}

		void SetMessage(MessageType msgType, char* pSetContents)
		{
			Type = msgType;
			pContents = pSetContents;
		}
	};


	enum class LogLevel
	{
		Trace = 0,
		Debug,
		Info,
		Warn,
		Error,
		Fetal
	};


	enum class NetResult : INT16
	{
		Success = 0,
				
	};

	class Helper
	{
		Helper() = default;
		~Helper() = default;

		INT32 GetAllocationGranularity()
		{
			SYSTEM_INFO systemInfo;
			GetSystemInfo(&systemInfo);
			return systemInfo.dwAllocationGranularity; ///< maybe 64K
		}

		INT32 SystemCoreCount()
		{
			SYSTEM_INFO systemInfo;
			GetSystemInfo(&systemInfo);
			return (INT32)systemInfo.dwNumberOfProcessors;
		}

		INT32 GetBestIOBufferSize(UINT32 Buffer)
		{
			auto AllocationGranularity = GetAllocationGranularity();
			return (INT32)((Buffer / AllocationGranularity + 1) * AllocationGranularity);
		}

		INT32 GetNumberOfProcessors()
		{
			SYSTEM_INFO		SysInfo;
			GetSystemInfo(&SysInfo);
			return (INT32)SysInfo.dwNumberOfProcessors;
		}
	};

	inline void (*LogFuncPtr) (const int, const char* pszLogMsg);
}