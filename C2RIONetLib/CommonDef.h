#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace C2RIONetLib
{
	struct ConnectionNetConfig
	{
		INT32 MaxRecvPacketSize = 0;
		UINT32 MaxTry1RecvBufferSize = 0;		
		INT32 MAXRecvCountPer1Sec = 0; // 정한 것 보다 조금 더 넉넉하게 주는 것을 추천
		INT32 MaxRecvBufferSize = 0;

		INT32 MaxSendPacketSize = 0;
		INT32 MAXSendCountPer1Sec = 0;  // 정한 것 보다 조금 더 넉넉하게 주는 것을 추천
		INT32 MaxSendBufferSize = 0;
		
	};

	struct NetConfig
	{
		UINT16 PortNumber = 0;
		INT32 IoWorkThreadCount = 0;

		INT32 MaxConnectionCount = 0;
		ConnectionNetConfig Conn;
	};

	enum class DisconnectReason
	{		
		None,
		Active,
		OnConnect_Request_Queue_Error,
		OnConnect_Error,
		Zero_Completion,
		IO_Request_Recv_Error,
		Partial_Send_Completion,
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
	public:
		Helper() = default;
		~Helper() = default;

		static INT32 GetAllocationGranularity()
		{
			SYSTEM_INFO systemInfo;
			GetSystemInfo(&systemInfo);
			return systemInfo.dwAllocationGranularity; ///< maybe 64K
		}

		static INT32 SystemCoreCount()
		{
			SYSTEM_INFO systemInfo;
			GetSystemInfo(&systemInfo);
			return (INT32)systemInfo.dwNumberOfProcessors;
		}

		static INT32 GetBestIOBufferSize(UINT32 Buffer)
		{
			auto AllocationGranularity = GetAllocationGranularity();
			return (INT32)((Buffer / AllocationGranularity + 1) * AllocationGranularity);
		}

		static INT32 GetNumberOfProcessors()
		{
			SYSTEM_INFO		SysInfo;
			GetSystemInfo(&SysInfo);
			return (INT32)SysInfo.dwNumberOfProcessors;
		}

		static const std::wstring s2ws(const std::string& s)
		{
			int slength = (int)s.length();
			int len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
			std::wstring r(len, L'\0');
			MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, &r[0], len);
			return r;
		}


		static const std::string ws2s(const std::wstring& s)
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