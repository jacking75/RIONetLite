#pragma once

// #define __USE_SEND_THREAD__ 1

#include "chat_server_content.h"
#include "logger.h"

#include <iostream>

using netErrorCode = rionetlite::NetErrorCode;

class ChatServer {
 public:
  ChatServer() = default;
  ~ChatServer() = default;

  bool Init(rionetlite::NetConfig netConfig) {
    Logger::Init();

    WriteLogNetConfig(netConfig);

    content_.Init(&net_mgr_);

    if (auto result = net_mgr_.Initialize(netConfig, &content_);
        result != netErrorCode::kNONE) {
      Logger::WriteLog(
          (int)rionetlite::LogLevel::kERROR,
          std::format("main() - netManager.Initialize() Error: {0}",
                      (int)result)
              .c_str());
      return false;
    }

    return true;
  }

  void Start() {
    net_mgr_.Start();

    content_.Start();

    std::this_thread::sleep_for(std::chrono::milliseconds(256));
  }

  void End() {
    net_mgr_.End();

    content_.Stop();

    Logger::WriteLog((int)rionetlite::LogLevel::kINFO, "End Server");
  }

  static void WriteLogNetConfig(rionetlite::NetConfig netConfig) {
    char logmsg[256] = {
        0,
    };

    sprintf_s(logmsg, "NetConfig - PortNumber: %d", netConfig.port_number_);
    Logger::WriteLog((int)rionetlite::LogLevel::kINFO, logmsg);

    sprintf_s(logmsg, "NetConfig - IoWorkThreadCount: %d",
              netConfig.io_work_thread_count_);
    Logger::WriteLog((int)rionetlite::LogLevel::kINFO, logmsg);

    sprintf_s(logmsg, "NetConfig - MaxConnectionCount: %d",
              netConfig.max_session_count_);
    Logger::WriteLog((int)rionetlite::LogLevel::kINFO, logmsg);

    sprintf_s(logmsg, "NetConfig - netConfig.Conn.MaxRecvPacketSize: %d",
              netConfig.session_cfg_.max_recv_packet_size_);
    Logger::WriteLog((int)rionetlite::LogLevel::kINFO, logmsg);

    sprintf_s(logmsg, "NetConfig - netConfig.Conn.MaxTry1RecvBufferSize: %d",
              netConfig.session_cfg_.max_try_1recv_buffer_size);
    Logger::WriteLog((int)rionetlite::LogLevel::kINFO, logmsg);

    sprintf_s(logmsg, "NetConfig - Conn.MAXRecvCountPer1Sec: %d",
              netConfig.session_cfg_.max_recv_count_per_1sec_);
    Logger::WriteLog((int)rionetlite::LogLevel::kINFO, logmsg);

    sprintf_s(logmsg, "NetConfig - Conn.MaxRecvBufferSize: %d",
              netConfig.session_cfg_.max_recv_buffer_size_);
    Logger::WriteLog((int)rionetlite::LogLevel::kINFO, logmsg);

    sprintf_s(logmsg, "NetConfig - Conn.MaxSendPacketSize: %d",
              netConfig.session_cfg_.max_send_packet_size_);
    Logger::WriteLog((int)rionetlite::LogLevel::kINFO, logmsg);

    sprintf_s(logmsg, "NetConfig - Conn.MAXSendCountPer1Sec: %d",
              netConfig.session_cfg_.max_send_count_per_1sec_);
    Logger::WriteLog((int)rionetlite::LogLevel::kINFO, logmsg);

    sprintf_s(logmsg, "NetConfig - Conn.MaxSendBufferSize: %d",
              netConfig.session_cfg_.max_send_buffer_size_);
    Logger::WriteLog((int)rionetlite::LogLevel::kINFO, logmsg);
  }

 private:
  ChatServerContent content_;
  rionetlite::ServerNetManager net_mgr_;
};
