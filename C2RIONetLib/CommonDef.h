#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <chrono>


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
		none,
		active,
		accept_loop_request_queue_error,
		accept_loop_on_connect_rrror,
		zero_completion,
		io_request_recv_error,
		partial_send_completion,
		send_error,
		forces_close_server,
		forces_close_send_error,
		end
	};
	char DisconnectReasonStr[(int)DisconnectReason::end][32] = { "none", "active", "accept_loop_request_queue_error", "accept_loop_on_connect_rrror", "zero_completion", "io_request_recv_error", "partial_send_completion", "send_error", "forces_close_server", "forces_close_send_error"  };
	
	enum class LogLevel
	{
		trace = 0,
		debug,
		info,
		warn,
		error,
		fetal,
		end
	};
	char LogLevelStr[(int)LogLevel::end][6] = { "trace", "debug", "info", "warn", "error", "fetal"  };


	enum class NetErrorCode : INT16
	{
		none = 0,
		session_count_per_thread_incorrect = 1,
		WSAStartUp_fail = 2,
		listen_socket_create_fail = 3,
		listen_socket_bind_fail = 5,
		rio_function_init_fail = 6,
		prepare_session_pool_fail = 7,
		listen_socket_listen_fail = 8,
		send_invalid_session = 9,
		send_io_fail = 10,
		send_session_network_disable = 11,
		recv_closed_status = 16,
		recv_disable_network_status = 17,
		recv_rio_fail = 18,
	};

	struct LazyedDisConnectSession
	{
		INT32 SessionIndex;
		UINT64 UniqueId;
		DisconnectReason reason;
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

		static INT64 CurrentMilliSecond()
		{
			auto curTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
			return curTime.count();
		}

		static INT64 CurrentSecond()
		{
			auto curTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch());
			return curTime.count();
		}
	};

	inline void (*LogFuncPtr) (const int, const char* pszLogMsg);
}