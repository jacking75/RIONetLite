#pragma once

#include "../../c2_rio_net/define.h"

class Logger
{
public:
	Logger() = default;
	~Logger() = default;

	static void Init()
	{
		c2rionet::LogFuncPtr = [](const int eLogInfoType, const char* szLogMsg) {
			WriteLog(eLogInfoType, szLogMsg);
		};
	}

	static void Debug(const char* szLogMsg)
	{
		auto level = (int)c2rionet::LogLevel::kDEBUG;
		printf("[%s] %s\r\n", c2rionet::LogLevelStr[level], szLogMsg);
	}

	static void Info(const char* szLogMsg)
	{
		auto level = (int)c2rionet::LogLevel::kINFO;
		printf("[%s] %s\r\n", c2rionet::LogLevelStr[level], szLogMsg);
	}

	static void Error(const char* szLogMsg)
	{
		auto level = (int)c2rionet::LogLevel::kERROR;
		printf("[%s] %s\r\n", c2rionet::LogLevelStr[level], szLogMsg);
	}

	static void WriteLog(const int eLogInfoType, const char* szLogMsg)
	{
		printf("[%s] %s\r\n", c2rionet::LogLevelStr[eLogInfoType], szLogMsg);
	}
};