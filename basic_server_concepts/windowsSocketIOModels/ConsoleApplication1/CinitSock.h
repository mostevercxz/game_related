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

// 每次接受一个新连接,就为该连接初始化一个 WSAEvent,并 new 一个 SOCKET_OBJ 存储进去
typedef struct _SOCKET_OBJ 
{
	SOCKET m_socket;
	HANDLE m_event;
	sockaddr_in m_remoteAddr;//客户端地址
	_SOCKET_OBJ *m_pNext;
}SOCKET_OBJ, *PSOCKET_OBJ;

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

// 定义线程对象，thread objects
typedef struct _THREAD_OBJ 
{
	HANDLE m_events[WSA_MAXIMUM_WAIT_EVENTS];//当前线程要等待的事件句柄集合
	int m_socketCount;//当前处理的套接字的数量 <= WSA_MAXIMUM_WAIT_EVENTS
	PSOCKET_OBJ m_head;//链表表头
	PSOCKET_OBJ m_tail;
	CRITICAL_SECTION m_cs;//关键代码段变量,同步对该结构的读写
	_THREAD_OBJ *m_pNext;
}THREAD_OBJ, *PTHREAD_OBJ;

PTHREAD_OBJ g_pThreadList;//指向线程链表表头
CRITICAL_SECTION g_cs;//同步对 g_pThreadList 的读写

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
	int n = 1;//从第1个开始写
	::LeaveCriticalSection(pThread->m_cs);
}
