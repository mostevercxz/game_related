#pragma once

#include <winsock2.h>
#include <WS2tcpip.h>
#include <vector>
#pragma comment(lib, "WS2_32")

const DWORD maxBufferLen = 1024;
const USHORT bindPort = 4567;
#define  WM_SOCKET 104

int do_select();
int do_wsaasync();
int do_wsaevent_single_thread();
int do_wsaevent_threadpool();

class CInitSock
{
public:
	CInitSock(const BYTE minorVer = 2, const BYTE majorVer = 2)
	{
		WSADATA wsaData;
		WORD sockVersion = MAKEWORD(minorVer, majorVer);
		if (::WSAStartup(sockVersion, &wsaData) != 0)
		{
			exit(0);
		}
	}

	~CInitSock()
	{
		::WSACleanup();
	}
};

// ÿ�ν���һ��������,��Ϊ�����ӳ�ʼ��һ�� WSAEvent,�� new һ�� SOCKET_OBJ �洢��ȥ
typedef struct _SOCKET_OBJ 
{
	SOCKET m_socket;
	HANDLE m_event;
	sockaddr_in m_remoteAddr;//�ͻ��˵�ַ
	_SOCKET_OBJ *m_pNext;
}SOCKET_OBJ, *PSOCKET_OBJ;

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

// �����̶߳���thread objects
typedef struct _THREAD_OBJ 
{
	HANDLE m_events[WSA_MAXIMUM_WAIT_EVENTS];//��ǰ�߳�Ҫ�ȴ����¼��������
	int m_socketCount;//��ǰ������׽��ֵ����� <= WSA_MAXIMUM_WAIT_EVENTS
	PSOCKET_OBJ m_head;//�����ͷ
	PSOCKET_OBJ m_tail;
	CRITICAL_SECTION m_cs;//�ؼ�����α���,ͬ���Ըýṹ�Ķ�д
	_THREAD_OBJ *m_pNext;
}THREAD_OBJ, *PTHREAD_OBJ;

PTHREAD_OBJ g_pThreadList;//ָ���߳������ͷ
CRITICAL_SECTION g_cs;//ͬ���� g_pThreadList �Ķ�д

PTHREAD_OBJ NewThreadObj()
{
	PTHREAD_OBJ pThread = (PTHREAD_OBJ)::GlobalAlloc(GPTR, sizeof(PTHREAD_OBJ));
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
	::EnterCriticalSection(pThread->m_cs);
	PTHREAD_OBJ pSocketHead = pThread->m_head;
	int n = 1;//�ӵ�1����ʼд
	::LeaveCriticalSection(pThread->m_cs);
}
