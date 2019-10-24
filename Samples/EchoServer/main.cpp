#include <iostream>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib,"mswsock.lib")
#pragma comment(lib,"DbgHelp.lib")
#pragma comment(lib, "winmm.lib")

#include "SimpleNetEventManager.h"

#include "../../C2RIONetLib/RIONetManager.h"

#include "../thirdparty/flags.h"

C2RIONetLib::NetConfig GetConfig();

int main()
{
	std::cout << "Start EchoServer" << std::endl;

	auto netConfig = GetConfig();

	C2RIONetLib::LogFuncPtr = [](const int eLogInfoType, const char* szLogMsg) {
		printf("[LogLevel:%d] %s\r\n", eLogInfoType, szLogMsg);
	};

	/// for dump on crash
	SetUnhandledExceptionFilter(C2RIONetLib::ExceptionFilter);

	SimpleNetEventManager netEventMgr;
	C2RIONetLib::RIONetManager<SimpleNetEventManager> netManager;
	
	if (netManager.Initialize(netConfig, &netEventMgr) == false) { return -1; }
			
	std::thread logicThread([&]() 
		{
			netManager.StartAndBlocking();
		}
	);
	

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

	// Á¾·á
	netManager.End();

	if (logicThread.joinable())
	{
		logicThread.join();
	}

	C2RIONetLib::LogFuncPtr((int)C2RIONetLib::LogLevel::Info, "End Server");
	return 0;
}

C2RIONetLib::NetConfig GetConfig()
{
	C2RIONetLib::NetConfig config;
	config.PortNumber = 11021;
	config.IoWorkThreadCount = 4;
	config.MaxConnectionCount = 256;
	
	config.Conn.MaxRecvPacketSize = 1024;
	config.Conn.MaxTry1RecvBufferSize = 2048;
	config.Conn.MAXRecvCountPer1Sec = 5;
	config.Conn.MaxRecvBufferSize = 4012 * 2;
	config.Conn.MaxSendPacketSize = 1024;
	config.Conn.MAXSendCountPer1Sec = 5;
	config.Conn.MaxSendBufferSize = 4012 * 2;

	return config;
}