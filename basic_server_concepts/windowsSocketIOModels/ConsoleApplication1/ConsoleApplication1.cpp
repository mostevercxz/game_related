// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "CinitSock.h"
#include "ciocp.h"


CInitSock theSock;

int main()
{
	//return do_select();
	//return do_wsaasync();
	//return do_wsaevent_threadpool();
	//return do_overlapped();
	CIOCPServer server;
	return do_simple_iocp();
}

