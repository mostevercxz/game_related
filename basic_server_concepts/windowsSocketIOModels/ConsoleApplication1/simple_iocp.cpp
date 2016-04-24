#include "CinitSock.h"

DWORD WINAPI SimpleIOCPThread(LPVOID lpParam)
{
	return 0;
}

int do_simple_iocp()
{
	HANDLE hCompletion = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	::CreateThread(NULL, 0, SimpleIOCPThread, (LPVOID)hCompletion, 0, 0);

	SOCKET sListen = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(bindPort);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;
	::bind(sListen, (const sockaddr*)&sin, sizeof(sin));
	::listen(sListen, 20);

	while (true)
	{

	}
	return 0;
}