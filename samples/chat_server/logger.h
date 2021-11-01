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

	static void WriteLog(const int eLogInfoType, const char* szLogMsg)
	{
		printf("[%s] %s\r\n", c2rionet::LogLevelStr[eLogInfoType], szLogMsg);
	}
};