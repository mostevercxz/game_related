#include "CinitSock.h"

int do_wsaevent_single_thread()
{
	WSAEVENT eventArray[WSA_MAXIMUM_WAIT_EVENTS];
	SOCKET sockArray[WSA_MAXIMUM_WAIT_EVENTS];
	int iEventTotal = 0;

	SOCKET sListen = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(bindPort);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;
	if (::bind(sListen, (const sockaddr*)&sin, sizeof(sin)) == SOCKET_ERROR)
	{
		printf("failed to bind,error:");
		return 1;
	}

	::listen(sListen, 5);
	//创建事件对象,并关联到套接字
	WSAEVENT event = ::WSACreateEvent();
	::WSAEventSelect(sListen, event, FD_ACCEPT | FD_CLOSE);
	// 添加到全局数组中
	eventArray[iEventTotal] = event;
	sockArray[iEventTotal] = sListen;
	++iEventTotal;

	//处理网络事件
	while (true)
	{
		int iIndex = ::WSAWaitForMultipleEvents(iEventTotal, eventArray, false, WSA_INFINITE, false);
		// 对每个事件调用 WSAWaitForMultipleEvents 函数,来确定它的状态

	}
}