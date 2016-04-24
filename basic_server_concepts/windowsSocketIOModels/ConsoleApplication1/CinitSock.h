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

// ÿ�ν���һ��������,��Ϊ�����ӳ�ʼ��һ�� WSAEvent,�� new һ�� SOCKET_OBJ �洢��ȥ
typedef struct _EVENT_MULTI_THREAD_SOCKET_OBJ 
{
	SOCKET m_socket;
	HANDLE m_event;
	sockaddr_in m_remoteAddr;//�ͻ��˵�ַ
	_EVENT_MULTI_THREAD_SOCKET_OBJ *m_pNext;
}EVENT_MULTI_THREAD_SOCKET_OBJ, *PEVENT_MULTI_THREAD_SOCKET_OBJ;



// �����̶߳���thread objects
typedef struct _THREAD_OBJ 
{
	HANDLE m_events[WSA_MAXIMUM_WAIT_EVENTS];//��ǰ�߳�Ҫ�ȴ����¼��������
	int m_socketCount;//��ǰ������׽��ֵ����� <= WSA_MAXIMUM_WAIT_EVENTS
	PEVENT_MULTI_THREAD_SOCKET_OBJ m_head;//�����ͷ
	PEVENT_MULTI_THREAD_SOCKET_OBJ m_tail;
	CRITICAL_SECTION m_cs;//�ؼ�����α���,ͬ���Ըýṹ�Ķ�д
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
	SOCKET m_socket;//���
	int m_iOverlappingNumber;//�ڴ��׽����ϵ��ص�IO����
	// ����� IO ���֮ǰ,�Է��ر������ӻ������Ӵ���,��Ҫ�ͷŶ�Ӧ�� OVERLAPPED_SOCKET_OBJ ����
	// ���ͷ�֮ǰ,���뱣֤���׽�����Ҳû���ص� IO ��,�� m_iOverlappingNumber = 0
	LPFN_ACCEPTEX m_acceptEx;//��չ���� AcceptEx ��ָ��(����Լ����׽��ֶ���)
} OVERLAPPED_SOCKET_OBJ, *POVERLAPPED_SOCKET_OBJ;

#define OP_ACCEPT 1
#define OP_READ 2
#define OP_WRITE 3

typedef struct _OVERLAPPED_BUFFER_OBJ
{
	OVERLAPPED m_ol;//�ص��ṹ
	char *m_buff;//send,recv,acceptex �Ļ�����
	int m_iLen;//buff�ĳ���
	POVERLAPPED_SOCKET_OBJ m_pSocket;//��IO�������׽��ֶ���
	int m_op;//�ύ�Ĳ�������
	SOCKET m_accpetSocket;//���� AcceptEx ���ܵĿͻ����׽���,����Լ����׽��ֶ���
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


