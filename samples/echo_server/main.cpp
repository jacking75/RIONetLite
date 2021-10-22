#include <iostream>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib,"mswsock.lib")
#pragma comment(lib,"DbgHelp.lib")
#pragma comment(lib, "winmm.lib")

#include "echo_server.h"

#include "../thirdparty/flags.h"

c2rionet::NetConfig GetConfig(int argc, char* argv[]);

int main(int argc, char* argv[])
{
	std::cout << "Start EchoServer" << std::endl;
			
	SetUnhandledExceptionFilter(c2rionet::ExceptionFilter);

	auto net_config = GetConfig(argc, argv);
	
	EchoServerImpl net_event_mgr;
	if (net_event_mgr.Init(net_config) == false)
	{
		return -1;
	}

	net_event_mgr.Start();
	
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
	net_event_mgr.End();

	return 0;
}

c2rionet::NetConfig GetConfig(int argc, char* argv[])
{
	c2rionet::NetConfig config;
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
	const auto port_number = args.get<UINT16>("PortNumber");
	if (!port_number) {
		std::cerr << "No PortNumber. :(" << std::endl;
		return config;
	}
	config.port_number_ = *port_number;

	const auto io_work_thread_count = args.get<UINT16>("IoWorkThreadCount");
	if (!io_work_thread_count) {
		std::cerr << "No IoWorkThreadCount. :(" << std::endl;
		return config;
	}
	config.io_work_thread_count_ = *io_work_thread_count;

	const auto max_connection_count = args.get<UINT16>("MaxConnectionCount");
	if (!max_connection_count) {
		std::cerr << "No MaxConnectionCount. :(" << std::endl;
		return config;
	}
	config.max_connection_count_ = *max_connection_count;

	const auto max_recv_packet_size = args.get<UINT16>("MaxRecvPacketSize");
	if (!max_recv_packet_size) {
		std::cerr << "No MaxRecvPacketSize. :(" << std::endl;
		return config;
	}
	config.conn_cfg_.max_recv_packet_size_ = *max_recv_packet_size;

	const auto max_try_1recv_buffer_size = args.get<UINT16>("MaxTry1RecvBufferSize");
	if (!max_try_1recv_buffer_size) {
		std::cerr << "No MaxTry1RecvBufferSize. :(" << std::endl;
		return config;
	}
	config.conn_cfg_.max_try_1recv_buffer_size = *max_try_1recv_buffer_size;

	const auto max_recv_count_per_1sec = args.get<UINT16>("MAXRecvCountPer1Sec");
	if (!max_recv_count_per_1sec) {
		std::cerr << "No MAXRecvCountPer1Sec. :(" << std::endl;
		return config;
	}
	config.conn_cfg_.max_recv_count_per_1sec_ = *max_recv_count_per_1sec;

	const auto max_recv_buffer_size = args.get<UINT16>("MaxRecvBufferSize");
	if (!max_recv_count_per_1sec) {
		std::cerr << "No MaxRecvBufferSize. :(" << std::endl;
		return config;
	}
	config.conn_cfg_.max_recv_buffer_size_ = *max_recv_buffer_size;

	const auto max_send_packet_size = args.get<UINT16>("MaxSendPacketSize");
	if (!max_recv_count_per_1sec) {
		std::cerr << "No MaxSendPacketSize. :(" << std::endl;
		return config;
	}
	config.conn_cfg_.max_send_packet_size_ = *max_send_packet_size;

	const auto max_send_count_per_1sec = args.get<UINT16>("MAXSendCountPer1Sec");
	if (!max_send_count_per_1sec) {
		std::cerr << "No MAXSendCountPer1Sec. :(" << std::endl;
		return config;
	}
	config.conn_cfg_.max_send_count_per_1sec_ = *max_send_count_per_1sec;

	const auto max_send_buffer_size = args.get<UINT16>("MaxSendBufferSize");
	if (!max_send_buffer_size) {
		std::cerr << "No MaxSendBufferSize. :(" << std::endl;
		return config;
	}
	config.conn_cfg_.max_send_buffer_size_ = *max_send_buffer_size;

	return config;
}