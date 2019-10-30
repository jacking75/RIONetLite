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
	/*config.PortNumber = 32452;
	config.IoWorkThreadCount = 4;
	config.MaxConnectionCount = 256;
	
	config.Conn.MaxRecvPacketSize = 4012;
	config.Conn.MaxTry1RecvBufferSize = 4096;
	config.Conn.MAXRecvCountPer1Sec = 5;
	config.Conn.MaxRecvBufferSize = 4096 * 3;
	config.Conn.MaxSendPacketSize = 4012;
	config.Conn.MAXSendCountPer1Sec = 5;
	config.Conn.MaxSendBufferSize = 4096 * 3;*/

	const flags::args args(argc, argv);
	const auto PortNumber = args.get<UINT16>("PortNumber");
	if (!PortNumber) {
		std::cerr << "No PortNumber. :(" << std::endl;
		return config;
	}
	config.PortNumber = *PortNumber;

	const auto IoWorkThreadCount = args.get<UINT16>("IoWorkThreadCount");
	if (!IoWorkThreadCount) {
		std::cerr << "No IoWorkThreadCount. :(" << std::endl;
		return config;
	}
	config.IoWorkThreadCount = *IoWorkThreadCount;

	const auto MaxConnectionCount = args.get<UINT16>("MaxConnectionCount");
	if (!MaxConnectionCount) {
		std::cerr << "No MaxConnectionCount. :(" << std::endl;
		return config;
	}
	config.MaxConnectionCount = *MaxConnectionCount;

	const auto MaxRecvPacketSize = args.get<UINT16>("MaxRecvPacketSize");
	if (!MaxRecvPacketSize) {
		std::cerr << "No MaxRecvPacketSize. :(" << std::endl;
		return config;
	}
	config.Conn.MaxRecvPacketSize = *MaxRecvPacketSize;

	const auto MaxTry1RecvBufferSize = args.get<UINT16>("MaxTry1RecvBufferSize");
	if (!MaxTry1RecvBufferSize) {
		std::cerr << "No MaxTry1RecvBufferSize. :(" << std::endl;
		return config;
	}
	config.Conn.MaxTry1RecvBufferSize = *MaxTry1RecvBufferSize;

	const auto MAXRecvCountPer1Sec = args.get<UINT16>("MAXRecvCountPer1Sec");
	if (!MAXRecvCountPer1Sec) {
		std::cerr << "No MAXRecvCountPer1Sec. :(" << std::endl;
		return config;
	}
	config.Conn.MAXRecvCountPer1Sec = *MAXRecvCountPer1Sec;

	const auto MaxRecvBufferSize = args.get<UINT16>("MaxRecvBufferSize");
	if (!MAXRecvCountPer1Sec) {
		std::cerr << "No MaxRecvBufferSize. :(" << std::endl;
		return config;
	}
	config.Conn.MaxRecvBufferSize = *MaxRecvBufferSize;

	const auto MaxSendPacketSize = args.get<UINT16>("MaxSendPacketSize");
	if (!MAXRecvCountPer1Sec) {
		std::cerr << "No MaxSendPacketSize. :(" << std::endl;
		return config;
	}
	config.Conn.MaxSendPacketSize = *MaxSendPacketSize;

	const auto MAXSendCountPer1Sec = args.get<UINT16>("MAXSendCountPer1Sec");
	if (!MAXSendCountPer1Sec) {
		std::cerr << "No MAXSendCountPer1Sec. :(" << std::endl;
		return config;
	}
	config.Conn.MAXSendCountPer1Sec = *MAXSendCountPer1Sec;

	const auto MaxSendBufferSize = args.get<UINT16>("MaxSendBufferSize");
	if (!MaxSendBufferSize) {
		std::cerr << "No MaxSendBufferSize. :(" << std::endl;
		return config;
	}
	config.Conn.MaxSendBufferSize = *MaxSendBufferSize;

	return config;
}