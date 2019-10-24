#pragma once

#include <WinSock2.h>
#include <MSWSock.h>

namespace C2RIONetLib
{
	enum IOType
	{
		IO_NONE,
		IO_SEND,
		IO_RECV,
	};

	

	enum RioConfig
	{
		//SESSION_BUFFER_SIZE = 65536,
		//MAX_RIO_THREAD = 4,
		//MAX_RIO_RESULT = 256,
		//MAX_SEND_RQ_SIZE_PER_SOCKET = 32,
		//MAX_RECV_RQ_SIZE_PER_SOCKET = 32,
		//MAX_CLIENT_PER_RIO_THREAD = 2560,
		//MAX_CQ_SIZE_PER_RIO_THREAD = (MAX_SEND_RQ_SIZE_PER_SOCKET + MAX_RECV_RQ_SIZE_PER_SOCKET) * MAX_CLIENT_PER_RIO_THREAD,

	};

	
	struct RioIoContext : public RIO_BUF
	{
		void Init(const int _session_index, IOType _ioType) 
		{
			session_index_ = _session_index;
			io_type_ = _ioType;
		}

		int session_index_;
		IOType	io_type_;
	};

	struct RIOAPI
	{
		inline static RIO_EXTENSION_FUNCTION_TABLE mRioFunctionTable = { 0, };

		static bool SetFunction(SOCKET socket)
		{
			/// RIO function table
			GUID functionTableId = WSAID_MULTIPLE_RIO;
			DWORD dwBytes = 0;

			if (WSAIoctl(socket, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER, &functionTableId, sizeof(GUID),
				(void**)&mRioFunctionTable, sizeof(mRioFunctionTable),
				&dwBytes, NULL, NULL))
			{
				return false;
			}

			return true;
		}
	};

#define RIO	RIOAPI::mRioFunctionTable

}

