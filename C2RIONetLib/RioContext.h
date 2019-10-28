#pragma once

#include <WinSock2.h>
#include <MSWSock.h>

namespace C2RIONetLib
{
	enum class IOType
	{
		none,
		send,
		recv,
	};
		
	struct RioIoContext : public RIO_BUF
	{
		void Init(const int sessionIndex_, IOType ioType_) 
		{
			Length = 0;

			SessionIndex = sessionIndex_;
			IoType = ioType_;
		}

		int SessionIndex;		
		IOType	IoType;
		UINT64 SessionUniqueId; // io 완료 시에 요청 때의 세션과 동일한지 체크하기 위해
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

