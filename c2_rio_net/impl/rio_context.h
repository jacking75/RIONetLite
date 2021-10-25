#pragma once

#include <WinSock2.h>
#include <MSWSock.h>

namespace c2rionet
{
	enum class IOType
	{
		kNONE,
		kSEND,
		kRECV,
	};
		
	struct RioIoContext : public RIO_BUF
	{
		void Init(const int session_index, IOType io_type) 
		{
			Length = 0;
			session_index_ = session_index;
			io_type_ = io_type;
		}

		int session_index_;		
		IOType	io_type_;
		UINT64 session_unique_Id_; // io 완료 시에 요청 때의 세션과 동일한지 체크하기 위해
	};

	struct RIOAPI
	{
		inline static RIO_EXTENSION_FUNCTION_TABLE rio_function_table_ = { 0, };

		static bool SetFunction(SOCKET socket)
		{
			/// RIO function table
			GUID functionTableId = WSAID_MULTIPLE_RIO;
			DWORD dwBytes = 0;

			if (WSAIoctl(socket, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER, &functionTableId, sizeof(GUID),
						(void**)&rio_function_table_, sizeof(rio_function_table_),
						&dwBytes, NULL, NULL)
				) {
				return false;
			}

			return true;
		}
	};

#define RIO	RIOAPI::rio_function_table_

}

