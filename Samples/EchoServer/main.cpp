#include <iostream>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib,"mswsock.lib")
#pragma comment(lib,"DbgHelp.lib")
#pragma comment(lib, "winmm.lib")

#include "../../C2RIONetLib/RIONetManager.h"

int main()
{
	C2RIONetLib::LogFuncPtr = [](const int eLogInfoType, const char* szLogMsg) {
		UNREFERENCED_PARAMETER(eLogInfoType);
		printf("%s\r\n", szLogMsg);
	};

	unsigned short port = 11021;
	//LIoThreadId = MAIN_THREAD_ID;

	/// for dump on crash
	SetUnhandledExceptionFilter(C2RIONetLib::ExceptionFilter);

	/// Global Managers
	//GSessionManager = new SessionManager;
	C2RIONetLib::RIONetManager GRioManager;
	
	if (GRioManager.Initialize(port) == false)
		return -1;
		
	if (GRioManager.StartIoThreads() == false)
		return -1;

	printf_s("Start Server\n");

	if (GRioManager.StartAcceptLoop() == false)
		return -1;

	printf_s("End Server\n");
		
	return 0;
}