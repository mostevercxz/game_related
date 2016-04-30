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
	//return do_simple_iocp();
	CMyServer *pServer = new CMyServer;

	// 开启服务
	if (pServer->Start())
	{
		printf(" 服务器开启成功... \n");
	}
	else
	{
		printf(" 服务器开启失败！\n");
		return 0;
	}

	// 创建事件对象，让ServerShutdown程序能够关闭自己
	HANDLE hEvent = ::CreateEvent(NULL, FALSE, FALSE, L"ShutdownEvent");
	::WaitForSingleObject(hEvent, INFINITE);
	::CloseHandle(hEvent);

	// 关闭服务
	pServer->Shutdown();
	delete pServer;

	printf(" 服务器关闭 \n ");
	return 0;
}

