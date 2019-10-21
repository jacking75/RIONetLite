#include <iostream>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib,"mswsock.lib")
#pragma comment(lib,"DbgHelp.lib")

#include "../../C2RIONetLib/RIONetManager.h"

int main()
{
	unsigned short port = 11021;
	//LIoThreadId = MAIN_THREAD_ID;

	/// for dump on crash
	SetUnhandledExceptionFilter(C2RIONetLib::ExceptionFilter);

	/// Global Managers
	//GSessionManager = new SessionManager;
	C2RIONetLib::RIONetManager GRioManager;
	
	if (GRioManager.Initialize(port) == false)
		return -1;

	/*if (false == GSessionManager->PrepareSessionPool())
		return -1;

	if (false == GRioManager->StartIoThreads())
		return -1;

	printf_s("Start Server\n");

	if (false == GRioManager->StartAcceptLoop())
		return -1;*/

	printf_s("End Server\n");
		
	return 0;
}