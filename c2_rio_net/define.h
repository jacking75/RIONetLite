#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <chrono>
#include <cstdint>

namespace c2rionet
{
	struct ConnectionNetConfig
	{
		std::int32_t max_recv_packet_size_ = 0;
		std::uint32_t max_try_1recv_buffer_size = 0;
		std::int32_t max_recv_count_per_1sec_ = 0; // 정한 것 보다 조금 더 넉넉하게 주는 것을 추천
		std::int32_t max_recv_buffer_size_ = 0;

		std::int32_t max_send_packet_size_ = 0;
		std::int32_t max_send_count_per_1sec_ = 0;  // 정한 것 보다 조금 더 넉넉하게 주는 것을 추천
		std::int32_t max_send_buffer_size_ = 0;
		
	};

	struct NetConfig
	{
		std::uint16_t port_number_ = 0;
		std::int32_t io_work_thread_count_ = 0;

		std::int32_t max_connection_count_ = 0;
		ConnectionNetConfig conn_cfg_;
	};

	

	enum class DisconnectReason
	{		
		kNONE,
		kACTIVE,
		kACCEPT_LOOP_REQUEST_QUEUE_ERROR,
		kkACCEPT_LOOP_ON_CONNECT_ERROR,
		kZERO_COMPLETION,
		kIO_REQUEST_RECV_ERROR,
		kPARITAL_SEND_COMPLETION,
		kSEND_ERROR,
		kFORCES_CLOSE_SERVER,
		kFORCES_CLOSE_SEND_ERROR,
		kEND
	};
	char DisconnectReasonStr[(int)DisconnectReason::kEND][32] = { "none", "active", "accept_loop_request_queue_error", "accept_loop_on_connect_rrror", "zero_completion", "io_request_recv_error", "partial_send_completion", "send_error", "forces_close_server", "forces_close_send_error"  };
	
	enum class LogLevel
	{
		kTRACE = 0,
		kDEBUG,
		kINFO,
		kWARN,
		kERROR,
		kFATAL,
		kEND
	};
	char LogLevelStr[(int)LogLevel::kEND][6] = { "trace", "debug", "info", "warn", "error", "fetal"  };


	enum class NetErrorCode : std::int16_t
	{
		kNONE = 0,
		kSESSION_COUNT_PER_THREAD_INCORRECT = 1,
		kWSASTARTUP_FAIL = 2,
		kLISTEN_SOCKET_CREATE_FAIL = 3,
		kLISTEN_SOCKET_BIND_FAIL = 5,
		kRIO_FUNCTION_INIT_FAIL = 6,
		kPREPARE_SESSION_POOL_FAIL = 7,
		kLISTEN_SOCKET_LISTEN_FAIL = 8,
		kSEND_INVALID_SESSION = 9,
		kSEND_IO_FAIL = 10,
		kSEND_SESSION_NETWORK_DISABLE = 11,
		kRECV_CLOSED_STATUS = 16,
		kRECV_DISABLE_NETWORK_STATUS = 17,
		kRECV_RIO_FAIL = 18,
	};

	struct LazyedDisConnectSession
	{
		std::int32_t session_index_ = -1;
		std::uint64_t unique_id_ = 0;
		DisconnectReason reason_ = DisconnectReason::kNONE;
	};
				
	inline void (*LogFuncPtr) (const int, const char* pszLogMsg);
}