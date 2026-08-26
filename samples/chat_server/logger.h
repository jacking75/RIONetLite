#pragma once

#include "../../RIONetLite/define.h"

class Logger {
 public:
  Logger() = default;
  ~Logger() = default;

  static void Init() {
    rionetlite::LogFuncPtr = [](const int eLogInfoType, const char* szLogMsg) {
      WriteLog(eLogInfoType, szLogMsg);
    };
  }

  static void Debug(const char* szLogMsg) {
    auto level = (int)rionetlite::LogLevel::kDEBUG;
    printf("[%s] %s\r\n", rionetlite::LogLevelStr[level], szLogMsg);
  }

  static void Info(const char* szLogMsg) {
    auto level = (int)rionetlite::LogLevel::kINFO;
    printf("[%s] %s\r\n", rionetlite::LogLevelStr[level], szLogMsg);
  }

  static void Error(const char* szLogMsg) {
    auto level = (int)rionetlite::LogLevel::kERROR;
    printf("[%s] %s\r\n", rionetlite::LogLevelStr[level], szLogMsg);
  }

  static void WriteLog(const int eLogInfoType, const char* szLogMsg) {
    printf("[%s] %s\r\n", rionetlite::LogLevelStr[eLogInfoType], szLogMsg);
  }
};