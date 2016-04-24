#pragma once

#include <winsock2.h>
#include <WS2tcpip.h>
#include <mswsock.h>
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
int do_overlapped();
int do_simple_iocp();

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
typedef struct _EVENT_MULTI_THREAD_SOCKET_OBJ 
{
	SOCKET m_socket;
	HANDLE m_event;
	sockaddr_in m_remoteAddr;//客户端地址
	_EVENT_MULTI_THREAD_SOCKET_OBJ *m_pNext;
}EVENT_MULTI_THREAD_SOCKET_OBJ, *PEVENT_MULTI_THREAD_SOCKET_OBJ;



// 定义线程对象，thread objects
typedef struct _THREAD_OBJ 
{
	HANDLE m_events[WSA_MAXIMUM_WAIT_EVENTS];//当前线程要等待的事件句柄集合
	int m_socketCount;//当前处理的套接字的数量 <= WSA_MAXIMUM_WAIT_EVENTS
	PEVENT_MULTI_THREAD_SOCKET_OBJ m_head;//链表表头
	PEVENT_MULTI_THREAD_SOCKET_OBJ m_tail;
	CRITICAL_SECTION m_cs;//关键代码段变量,同步对该结构的读写
	_THREAD_OBJ *m_pNext;

	_THREAD_OBJ() : m_socketCount(0), m_head(NULL), 
		m_tail(NULL), m_pNext(NULL)
	{
		memset(m_events, 0, sizeof(m_events));
	}
}THREAD_OBJ, *PTHREAD_OBJ;


// overlapped io structure defined
typedef struct _OVERLAPPED_SOCKER_OBJ 
{
	SOCKET m_socket;//句柄
	int m_iOverlappingNumber;//在此套接字上的重叠IO数量
	// 如果在 IO 完成之前,对方关闭了连接或者连接错误,就要释放对应的 OVERLAPPED_SOCKET_OBJ 对象
	// 但释放之前,必须保证此套接字再也没有重叠 IO 了,即 m_iOverlappingNumber = 0
	LPFN_ACCEPTEX m_acceptEx;//扩展函数 AcceptEx 的指针(仅针对监听套接字而言)
} OVERLAPPED_SOCKET_OBJ, *POVERLAPPED_SOCKET_OBJ;

#define OP_ACCEPT 1
#define OP_READ 2
#define OP_WRITE 3

typedef struct _OVERLAPPED_BUFFER_OBJ
{
	OVERLAPPED m_ol;//重叠结构
	char *m_buff;//send,recv,acceptex 的缓冲区
	int m_iLen;//buff的长度
	POVERLAPPED_SOCKET_OBJ m_pSocket;//次IO所属的套接字对象
	int m_op;//提交的操作类型
	SOCKET m_accpetSocket;//保存 AcceptEx 接受的客户端套接字,仅针对监听套接字而言
	_OVERLAPPED_BUFFER_OBJ *m_pNext;
} OVERLAPPED_BUFFER_OBJ, *POVERLAPPED_BUFFER_OBJ;


// simple iocp example
typedef struct _PER_HANDLE_DATA
{
	SOCKET m_socket;
	sockaddr_in m_clientAddr;
}PER_HANDLE_DATA, *PPER_HANDLE_DATA;

typedef struct _PER_IO_DATA
{
	OVERLAPPED m_overlapped;
	char m_buff[maxBufferLen];
	int m_op;
}PER_IO_DATA, *PPER_IO_DATA;


