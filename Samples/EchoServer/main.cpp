#include <iostream>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib,"mswsock.lib")
#pragma comment(lib,"DbgHelp.lib")
#pragma comment(lib, "winmm.lib")

#include "EchoServer.h"

#include "../thirdparty/flags.h"

C2RIONetLib::NetConfig GetConfig(int argc, char* argv[]);

int main(int argc, char* argv[])
{
	std::cout << "Start EchoServer" << std::endl;
			
	SetUnhandledExceptionFilter(C2RIONetLib::ExceptionFilter);

	auto netConfig = GetConfig(argc, argv);
	
	EchoServerImpl netEventMgr;
	if (netEventMgr.Init(netConfig) == false)
	{
		return -1;
	}

	netEventMgr.Start();
	
	std::cout << "Server quits when you press (quit) !!!" << std::endl;	
	while (true)
	{
		std::string inputCmd;
		std::getline(std::cin, inputCmd);

		if (inputCmd == "quit")
		{
			break;
		}
	}

	// 종료
	netEventMgr.End();

	return 0;
}

C2RIONetLib::NetConfig GetConfig(int argc, char* argv[])
{
	C2RIONetLib::NetConfig config;
	config.PortNumber = 32452;
	config.IoWorkThreadCount = 4;
	config.MaxConnectionCount = 256;
	
	config.Conn.MaxRecvPacketSize = 1024;
	config.Conn.MaxTry1RecvBufferSize = 2048;
	config.Conn.MAXRecvCountPer1Sec = 5;
	config.Conn.MaxRecvBufferSize = 4096 * 2;
	config.Conn.MaxSendPacketSize = 1024;
	config.Conn.MAXSendCountPer1Sec = 5;
	config.Conn.MaxSendBufferSize = 4096 * 2;

	/*const flags::args args(argc, argv);
	const auto port = args.get<UINT16>("port");
	if (!port) {
		std::cerr << "No Port. :(" << std::endl;
		return config;
	}*/

	return config;
}