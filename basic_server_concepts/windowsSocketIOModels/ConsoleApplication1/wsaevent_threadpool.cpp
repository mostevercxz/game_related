#include "CinitSock.h"

LONG g_totalConnectionsNumber;//�ܹ���������
LONG g_currentConnectionsNumber;//��ǰ��������
PTHREAD_OBJ g_pThreadList;//ָ���߳������ͷ
CRITICAL_SECTION g_cs;//ͬ���� g_pThreadList �Ķ�д

								// new a SOCKET_OBJ class and associate the socket with a WSAEvent
PSOCKET_OBJ NewSocketObj(SOCKET s)
{
	// GPTR ��һ�����,�������һ��̶���С���ڴ�,���ڴ�memsetΪ0
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
	int n = 1;//�ӵ�1����ʼд
	while (pSocketHead)
	{
		pThread->m_events[n++] = pSocketHead->m_event;
		pSocketHead = pSocketHead->m_pNext;
	}
	::LeaveCriticalSection(&pThread->m_cs);
}

// ���� index ȥ�߳����� pSocket,�Ҳ������� NULL
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
		// �������ֻ��1���ڵ�,��ôͷ��β��Ҫ�ı�
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


// ����IO
bool HandleIO(PTHREAD_OBJ pThread, PSOCKET_OBJ pSocket)
{
	// ��þ���������¼�
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
		// �����Ŷ���ʼ,�����������,�鿴�Ƿ�����
		for (int i = iIndex; i < pThread->m_socketCount + 1;++i)
		{
			iIndex = ::WSAWaitForMultipleEvents(1, pThread->m_events + i, true, 1000, false);
			if (iIndex == WSA_WAIT_TIMEOUT || iIndex == WSA_WAIT_FAILED)
			{
				continue;
			}
			else
			{
				if (0 == i)//events[0] ����,�ؽ�����
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
					// ���Ҷ�Ӧ�� socket, �����ö�Ӧ�����ݴ�����
					PSOCKET_OBJ pSocket = FindSocketByIndex(pThread, i);
					if (!pSocket)
					{
						printf("cound not find socket at index %d", i);
					}
					else
					{
						// return false �����ѳ��ִ���,�� pSocket �ѱ��Ƴ���
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

// �� socket ���뵽ĳ�̵߳��¼���
// �ϲ㺯���Ѿ�Ϊ�ú����ҵ�һ�����ʵ��߳�
void InsertSocketObj(PTHREAD_OBJ pThread, PSOCKET_OBJ pSocket)
{
	// �� pSocket ���뵽 pThread��������,����+1	
	::EnterCriticalSection(&pThread->m_cs);
	// �ϲ㺯����������Ƿ�ﵽ��������
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

	// ��ȫ�ֱ�����������1
	::InterlockedIncrement(&g_totalConnectionsNumber);
	::InterlockedIncrement(&g_currentConnectionsNumber);
}

// ��һ���׽��ְ��Ÿ�һ��������߳�
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

	// �Ҳ������е��߳�,����
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
	// ��ʾ�߳��ؽ��������
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

	// �����¼�����,���������������׽���
	WSAEVENT event = ::WSACreateEvent();
	::WSAEventSelect(sListen, event, FD_ACCEPT | FD_CLOSE);
	::InitializeCriticalSection(&g_cs);
	while (true)
	{
		//int iRet = ::WaitForSingleObject(event, 5 * 1000);//ΪɶҪ�ȴ����Object
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
			// ��������δ��������
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