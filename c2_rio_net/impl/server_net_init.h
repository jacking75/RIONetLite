#pragma once

#include "..\define.h"
#include "rio_context.h"

#include <WinSock2.h>


namespace c2rionet
{
struct ServerNetManagerInit
{
	static NetErrorCode Initialize(SOCKET& listen_socket, NetConfig net_cfg)
	{
		if ((net_cfg.max_session_count_ % net_cfg.io_work_thread_count_) != 0) {
			return NetErrorCode::kSESSION_COUNT_PER_THREAD_INCORRECT;
		}
			
		if (WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
			return NetErrorCode::kWSASTARTUP_FAIL;
		}

		if (auto result = ServerNetManagerInit::BindListenSocket(listen_socket, net_cfg.port_number_);
			result != NetErrorCode::kNONE) {
			return result;
		}

		if (RIOAPI::SetFunction(listen_socket) == false) { 
			return NetErrorCode::kRIO_FUNCTION_INIT_FAIL; 
		}

		if (SOCKET_ERROR == listen(listen_socket, SOMAXCONN)) {
			return NetErrorCode::kLISTEN_SOCKET_LISTEN_FAIL; 
		}
						
		return NetErrorCode::kNONE;
	}

	static NetErrorCode BindListenSocket(SOCKET& listen_socket, const UINT16 port)
	{
		/// create TCP socket with RIO mode
		listen_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_REGISTERED_IO);
		if (listen_socket == INVALID_SOCKET) {
			return NetErrorCode::kLISTEN_SOCKET_CREATE_FAIL;
		}

		int opt = 1;
		setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(int));

		SOCKADDR_IN serveraddr;
		ZeroMemory(&serveraddr, sizeof(serveraddr));
		serveraddr.sin_family = AF_INET;
		serveraddr.sin_port = htons(port);
		serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

		if (SOCKET_ERROR == bind(listen_socket, (SOCKADDR*)&serveraddr, sizeof(serveraddr))) {
			return NetErrorCode::kLISTEN_SOCKET_BIND_FAIL;
		}

		char logmsg[256] = { 0, };
		sprintf_s(logmsg, "success Bind, Listen - Port: %d", port);
		LogFuncPtr((int)LogLevel::kINFO, logmsg);
		return NetErrorCode::kNONE;
	}
};

}// namespace c2rionet