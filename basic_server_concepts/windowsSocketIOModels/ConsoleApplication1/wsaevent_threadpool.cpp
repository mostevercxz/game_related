#include "CinitSock.h"

LONG g_totalConnectionsNumber;//总共连接数量
LONG g_currentConnectionsNumber;//当前连接数量
PTHREAD_OBJ g_pThreadList;//指向线程链表表头
CRITICAL_SECTION g_cs;//同步对 g_pThreadList 的读写

								// new a SOCKET_OBJ class and associate the socket with a WSAEvent
PSOCKET_OBJ NewSocketObj(SOCKET s)
{
	// GPTR 是一个标记,代表分配一块固定大小的内存,将内存memset为0
	PSOCKET_OBJ pSocket = (PSOCKET_OBJ)::GlobalAlloc(GPTR, sizeof(SOCKET_OBJ));
	if (pSocket)
	{
		pSocket->m_socket = s;
		pSocket->m_event = ::WSACreateEvent();
	}
	return pSocket;
}

void FreeSocketObj(PSOCKET_OBJ pSocket)
{
	::CloseHandle(pSocket->m_event);
	if (pSocket->m_socket != INVALID_SOCKET)
	{
		::closesocket(pSocket->m_socket);
	}
	::GlobalFree(pSocket);
}

PTHREAD_OBJ NewThreadObj()
{
	PTHREAD_OBJ pThread = (PTHREAD_OBJ)::GlobalAlloc(GPTR, sizeof(THREAD_OBJ));	
	if (pThread)
	{
		::InitializeCriticalSection(&pThread->m_cs);
		pThread->m_events[0] = ::WSACreateEvent();
		::EnterCriticalSection(&g_cs);
		pThread->m_pNext = g_pThreadList;
		g_pThreadList = pThread;
		::LeaveCriticalSection(&g_cs);
	}

	return pThread;
}

void FreeThreadObj(PTHREAD_OBJ pThread)
{
	::EnterCriticalSection(&g_cs);
	PTHREAD_OBJ p = g_pThreadList;
	if (p == pThread)
	{
		g_pThreadList = pThread->m_pNext;
	}
	else
	{
		while (p && p->m_pNext != pThread)
		{
			p = p->m_pNext;
		}
		if (p)
		{
			p->m_pNext = pThread->m_pNext;
		}
	}
	::LeaveCriticalSection(&g_cs);

	::CloseHandle(pThread->m_events[0]);
	::DeleteCriticalSection(&pThread->m_cs);
	::GlobalFree(pThread);
}

void RebuildArray(PTHREAD_OBJ pThread)
{
	::EnterCriticalSection(&pThread->m_cs);
	PSOCKET_OBJ pSocketHead = pThread->m_head;
	int n = 1;//从第1个开始写
	while (pSocketHead)
	{
		pThread->m_events[n++] = pSocketHead->m_event;
		pSocketHead = pSocketHead->m_pNext;
	}
	::LeaveCriticalSection(&pThread->m_cs);
}

// 根据 index 去线程中找 pSocket,找不到返回 NULL
PSOCKET_OBJ FindSocketByIndex(PTHREAD_OBJ pThread, int index)
{
	PSOCKET_OBJ pSocket = pThread->m_head;
	while (--index)
	{
		if (!pSocket)
		{
			return NULL;
		}
		pSocket = pSocket->m_pNext;
	}

	return pSocket;
}

void RemoveSocketObj(PTHREAD_OBJ pThread, PSOCKET_OBJ pSocket)
{
	::EnterCriticalSection(&pThread->m_cs);
	PSOCKET_OBJ pHeader = pThread->m_head;
	if (pHeader == pSocket)
	{
		// 如果链表只有1个节点,那么头和尾都要改变
		if (pThread->m_head == pThread->m_tail)
		{
			pThread->m_head = pThread->m_tail = pHeader->m_pNext;
		}
		else
		{
			pThread->m_head = pHeader->m_pNext;
		}
	}
	else
	{
		while (pHeader && pHeader->m_pNext != pSocket)
		{
			pHeader = pHeader->m_pNext;
		}

		if (pHeader)
		{
			if (pThread->m_tail == pSocket)
			{
				pThread->m_tail = pHeader;
			}
			pHeader->m_pNext = pSocket->m_pNext;
		}
	}
	--(pThread->m_socketCount);
	::LeaveCriticalSection(&pThread->m_cs);

	::WSASetEvent(pThread->m_events[0]);
	::InterlockedDecrement(&g_currentConnectionsNumber);
}


// 处理IO
bool HandleIO(PTHREAD_OBJ pThread, PSOCKET_OBJ pSocket)
{
	// 获得具体的网络事件
	WSANETWORKEVENTS event;
	::WSAEnumNetworkEvents(pSocket->m_socket, pSocket->m_event, &event);
	bool needRemoveSocket = false;
	if (event.lNetworkEvents & FD_READ)
	{
		if (event.iErrorCode[FD_READ_BIT] == 0)
		{
			char buffer[maxBufferLen] = { 0 };
			int iRecv = ::recv(pSocket->m_socket, buffer, sizeof(buffer), 0);
			if (iRecv > 0)
			{
				buffer[iRecv] = '\0';
				printf("receive data : %s\n", buffer);
			}
		}
		else
		{
			needRemoveSocket = true;
		}
	}
	else if (event.lNetworkEvents & FD_CLOSE)
	{
		needRemoveSocket = true;
	}	
	else if (event.lNetworkEvents & FD_WRITE)
	{
		if (event.iErrorCode[FD_WRITE_BIT] == 0)
		{
			// do nothing
		}
		else
		{
			needRemoveSocket = true;
		}
	}

	if (needRemoveSocket)
	{
		RemoveSocketObj(pThread, pSocket);
		FreeSocketObj(pSocket);
		return false;
	}


	return true;
}

DWORD WINAPI ServerThread(LPVOID lpParam)
{
	PTHREAD_OBJ pThread = (PTHREAD_OBJ)lpParam;
	while (true)
	{
		int iIndex = ::WSAWaitForMultipleEvents(
			pThread->m_socketCount + 1, pThread->m_events,
			false, WSA_INFINITE, false);

		iIndex -= WSA_WAIT_EVENT_0;
		// 从受信对象开始,依次往后遍历,查看是否受信
		for (int i = iIndex; i < pThread->m_socketCount + 1;++i)
		{
			iIndex = ::WSAWaitForMultipleEvents(1, pThread->m_events + i, true, 1000, false);
			if (iIndex == WSA_WAIT_TIMEOUT || iIndex == WSA_WAIT_FAILED)
			{
				continue;
			}
			else
			{
				if (0 == i)//events[0] 受信,重建数组
				{
					RebuildArray(pThread);
					if (0 == pThread->m_socketCount)
					{
						FreeThreadObj(pThread);
						return 0;
					}
					::WSAResetEvent(pThread->m_events[0]);
				}
				else
				{
					// 查找对应的 socket, 并调用对应的数据处理函数
					PSOCKET_OBJ pSocket = FindSocketByIndex(pThread, i);
					if (!pSocket)
					{
						printf("cound not find socket at index %d", i);
					}
					else
					{
						// return false 代表已出现错误,该 pSocket 已被移除掉
						if (!HandleIO(pThread, pSocket))
						{
							RebuildArray(pThread);
						}
					}
				}
			}
		}
	}
	return 0;
}

// 将 socket 插入到某线程的事件中
// 上层函数已经为该函数找到一个合适的线程
void InsertSocketObj(PTHREAD_OBJ pThread, PSOCKET_OBJ pSocket)
{
	// 将 pSocket 插入到 pThread的链表中,个数+1	
	::EnterCriticalSection(&pThread->m_cs);
	// 上层函数检查数量是否达到容纳上限
	if (!pThread->m_head)
	{
		pThread->m_head = pSocket;
		pThread->m_tail = pSocket;
	}
	else
	{
		pThread->m_tail->m_pNext = pSocket;
		pThread->m_tail = pSocket;
	}
	++(pThread->m_socketCount);
	::LeaveCriticalSection(&pThread->m_cs);

	// 将全局变量计数都加1
	::InterlockedIncrement(&g_totalConnectionsNumber);
	::InterlockedIncrement(&g_currentConnectionsNumber);
}

// 将一个套接字安排给一个空余的线程
void AssignForFreeThread(PSOCKET_OBJ pSocket)
{
	pSocket->m_pNext = NULL;
	::EnterCriticalSection(&g_cs);
	PTHREAD_OBJ pThread = g_pThreadList;
	while (pThread)
	{
		if (pThread->m_socketCount < WSA_MAXIMUM_WAIT_EVENTS - 1)
		{
			InsertSocketObj(pThread, pSocket);
			break;
		}
		pThread = pThread->m_pNext;
	}

	// 找不到空闲的线程,分配
	if (!pThread)
	{
		pThread = NewThreadObj();
		if (!pThread)
		{
			printf("fatal error, can not allocate memory for thread");
			return;
		}
		InsertSocketObj(pThread, pSocket);
		::CreateThread(NULL, 0, ServerThread, pThread, 0, NULL);
	}
	::LeaveCriticalSection(&g_cs);
	// 提示线程重建句柄数组
	::WSASetEvent(pThread->m_events[0]);
}


int do_wsaevent_threadpool()
{
	// S,B,L,A
	SOCKET sListen = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(bindPort);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;
	if (::bind(sListen, (const sockaddr*)&sin, sizeof(sin)) == SOCKET_ERROR)
	{
		printf("bind socket error");
		return -1;
	}
	::listen(sListen, 20);

	// 创建事件对象,并关联到监听的套接字
	WSAEVENT event = ::WSACreateEvent();
	::WSAEventSelect(sListen, event, FD_ACCEPT | FD_CLOSE);
	::InitializeCriticalSection(&g_cs);
	while (true)
	{
		//int iRet = ::WaitForSingleObject(event, 5 * 1000);//为啥要等待这个Object
		int iRet = ::WSAWaitForMultipleEvents(1, &event, true, 1000, false);
		if (iRet == WSA_WAIT_FAILED)
		{
			printf("WSAWaitForMultipleEvents() failed");
			break;
		}
		else if (iRet == WSA_WAIT_TIMEOUT)
		{
			//printf("WSAWaitForMultipleEvents() timeout");
			continue;
		}
		else
		{
			printf("WSAWaitForMultipleEvents() got result");
			::ResetEvent(event);
			// 处理所有未决的请求
			while (true)
			{
				sockaddr_in tmp;				
				int nLen = sizeof(tmp);
				SOCKET acceptRet = ::accept(sListen, (struct sockaddr*)&tmp, &nLen);
				//SOCKET acceptRet = ::accept(sListen, NULL, NULL);				
				if (acceptRet == SOCKET_ERROR)
				{
					int errorCode = WSAGetLastError();
					printf("accept,errorcode=%d", errorCode);
					break;
				}
				PSOCKET_OBJ pSocket = NewSocketObj(acceptRet);
				pSocket->m_remoteAddr = tmp;
				::WSAEventSelect(pSocket->m_socket, pSocket->m_event, FD_READ | FD_CLOSE | FD_WRITE);
				AssignForFreeThread(pSocket);
			}
		}
	}
	
	::DeleteCriticalSection(&g_cs);

	return 0;
}