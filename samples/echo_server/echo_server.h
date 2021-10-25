#pragma once

#include "echo_server_content.h"

#include <iostream>

using netErrorCode = c2rionet::NetErrorCode;



class EchoServer
{
public:
	EchoServer() = default;
	~EchoServer() = default;

	bool Init(c2rionet::NetConfig netConfig)
	{	
		c2rionet::LogFuncPtr = [](const int eLogInfoType, const char* szLogMsg) {
			EchoServer::WriteLog(eLogInfoType, szLogMsg);
		};

		EchoServer::WriteToLog(netConfig);

		content_.pref_net_mgr_ = &net_mgr_;
		if (auto result = net_mgr_.Initialize(netConfig, &content_); result != netErrorCode::kNONE) {
			WriteLog_Initialize("main() - netManager.Initialize() Error: %d", result);
			return false;
		}

		return true;
	}

	void Start()
	{
		logic_thread_ = std::thread([&]() { 
				net_mgr_.StartAndBlocking(); 
			});

		std::this_thread::sleep_for(std::chrono::milliseconds(256));
	}

	void End()
	{
		net_mgr_.End();

		if (logic_thread_.joinable()) {
			logic_thread_.join();
		}

		WriteLog((int)c2rionet::LogLevel::kINFO, "End Server");
	}

	static void WriteToLog(c2rionet::NetConfig netConfig)
	{
		char logmsg[256] = { 0, };

		sprintf_s(logmsg, "NetConfig - PortNumber: %d", netConfig.port_number_);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);

		sprintf_s(logmsg, "NetConfig - IoWorkThreadCount: %d", netConfig.io_work_thread_count_);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);

		sprintf_s(logmsg, "NetConfig - MaxConnectionCount: %d", netConfig.max_connection_count_);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);

		sprintf_s(logmsg, "NetConfig - netConfig.Conn.MaxRecvPacketSize: %d", netConfig.conn_cfg_.max_recv_packet_size_);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);

		sprintf_s(logmsg, "NetConfig - netConfig.Conn.MaxTry1RecvBufferSize: %d", netConfig.conn_cfg_.max_try_1recv_buffer_size);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);

		sprintf_s(logmsg, "NetConfig - Conn.MAXRecvCountPer1Sec: %d", netConfig.conn_cfg_.max_recv_count_per_1sec_);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);

		sprintf_s(logmsg, "NetConfig - Conn.MaxRecvBufferSize: %d", netConfig.conn_cfg_.max_recv_buffer_size_);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);

		sprintf_s(logmsg, "NetConfig - Conn.MaxSendPacketSize: %d", netConfig.conn_cfg_.max_send_packet_size_);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);

		sprintf_s(logmsg, "NetConfig - Conn.MAXSendCountPer1Sec: %d", netConfig.conn_cfg_.max_send_count_per_1sec_);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);

		sprintf_s(logmsg, "NetConfig - Conn.MaxSendBufferSize: %d", netConfig.conn_cfg_.max_send_buffer_size_);
		WriteLog((int)c2rionet::LogLevel::kINFO, logmsg);
	}

	static void WriteLog(const int eLogInfoType, const char* szLogMsg)
	{
		printf("[%s] %s\r\n", c2rionet::LogLevelStr[eLogInfoType], szLogMsg);
	}

	static void WriteLog_Initialize(const char* sz_format, const netErrorCode error)
	{
		char logmsg[256] = { 0, };
		sprintf_s(logmsg, sz_format, error);
		WriteLog((int)c2rionet::LogLevel::kERROR, logmsg);
	}
	


private:
	EchoServerContent content_;
	c2rionet::ServerNetManager net_mgr_;
	std::thread logic_thread_;
};
