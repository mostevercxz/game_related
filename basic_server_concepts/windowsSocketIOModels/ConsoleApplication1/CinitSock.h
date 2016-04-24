#pragma once

#include <winsock2.h>
#include <WS2tcpip.h>
#include <vector>
#include <string.h>
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



// 定义线程对象，thread objects
typedef struct _THREAD_OBJ 
{
	HANDLE m_events[WSA_MAXIMUM_WAIT_EVENTS];//当前线程要等待的事件句柄集合
	int m_socketCount;//当前处理的套接字的数量 <= WSA_MAXIMUM_WAIT_EVENTS
	PSOCKET_OBJ m_head;//链表表头
	PSOCKET_OBJ m_tail;
	CRITICAL_SECTION m_cs;//关键代码段变量,同步对该结构的读写
	_THREAD_OBJ *m_pNext;

	_THREAD_OBJ() : m_socketCount(0), m_head(NULL), 
		m_tail(NULL), m_pNext(NULL)
	{
		memset(m_events, 0, sizeof(m_events));
	}
}THREAD_OBJ, *PTHREAD_OBJ;




