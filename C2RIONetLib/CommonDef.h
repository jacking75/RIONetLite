#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace C2RIONetLib
{
	struct ConnectionNetConfig
	{
		INT32 MaxRecvPacketSize = 0;
		UINT32 MaxTry1RecvBufferSize = 0;		
		INT32 MAXRecvPacketCountPer1Sec = 0;
		INT32 MaxRecvBufferSize = 0;

		INT32 MaxSendPacketSize = 0;
		INT32 MAXSendCountPer1Sec = 0;
		INT32 MaxSendBufferSize = 0;
		
	};

	struct NetConfig
	{
		UINT16 PortNumber = 0;
		INT32 IoWorkThreadCount = 0;

		INT32 MaxConnectionCount = 0;
		ConnectionNetConfig Conn;
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

		const std::wstring s2ws(const std::string& s)
		{
			int slength = (int)s.length();
			int len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
			std::wstring r(len, L'\0');
			MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, &r[0], len);
			return r;
		}


		const std::string ws2s(const std::wstring& s)
		{
			int slength = (int)s.length();
			int len = WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, 0, 0, 0, 0);
			std::string r(len, '\0');
			WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, &r[0], len, 0, 0);
			return r;
		}
	};

	inline void (*LogFuncPtr) (const int, const char* pszLogMsg);
}