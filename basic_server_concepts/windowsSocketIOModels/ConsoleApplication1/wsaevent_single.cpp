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
        iIndex = iIndex - WSA_WAIT_EVENT_0;
        for(int i = iIndex; i < iEventTotal; ++i)
        {
            iIndex = ::WSAWaitForMultipleEvents(1, &eventArray[i], TRUE, 1000, FALSE);
            if (iIndex == WSA_WAIT_FAILED || iIndex == WSA_WAIT_TIMEOUT)
            {
                continue;
            }
            else
            {
                // 获取到来的消息通知, WSAEnumNetworkEvents 会自动重置受信事件
                WSANETWORKEVENTS tmpEvent;
                ::WSAEnumNetworkEvents(sockArray[i], eventArray[i], &tmpEvent);
                if (tmpEvent.lNetworkEvents & FD_ACCEPT)
                {
                    if (tmpEvent.iErrorCode[FD_ACCEPT_BIT] == 0)
                    {
                        if (iEventTotal > WSA_MAXIMUM_WAIT_EVENTS)
                        {
                            printf("too many connections, max available is %u", WSA_MAXIMUM_WAIT_EVENTS);
                            continue;
                        }
                        
                        SOCKET sNew = ::accept(sockArray[i], NULL, NULL);
                        WSAEVENT newEvent = ::WSACreateEvent();
                        ::WSAEventSelect(sNew, newEvent, FD_READ | FD_CLOSE | FD_WRITE);
                        sockArray[iEventTotal] = sNew;
                        eventArray[iEventTotal] = newEvent;
                        ++iEventTotal;
                    }
                }
                else if(tmpEvent.lNetworkEvents & FD_READ)
                {
                    if (tmpEvent.iErrorCode[FD_READ_BIT] == 0)
                    {
                        char buffer[maxBufferLen] = {0};
                        int nRecv = ::recv(sockArray[i], buffer, sizeof(buffer), 0);
                        if (nRecv > 0)
                        {
                            buffer[nRecv] = '\0';
                            printf("receive from buffer:%s", buffer);
                        }
                    }
                }
                else if (tmpEvent.lNetworkEvents & FD_CLOSE)
                {
                    if (tmpEvent.iErrorCode[FD_CLOSE_BIT] == 0)
                    {
                        ::closesocket(sockArray[i]);
                        for (int j = i; j < iEventTotal - 1; ++j)
                        {
                            sockArray[j] = sockArray[j+1];
                            eventArray[j] = eventArray[j+1];
                        }
                        --iEventTotal;
                    }
                }
                else if (tmpEvent.lNetworkEvents & FD_WRITE)
                {}
            }
        }
	}
    
    return 0;
}