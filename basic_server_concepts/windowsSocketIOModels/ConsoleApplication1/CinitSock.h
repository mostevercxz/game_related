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

// ÿ�ν���һ��������,��Ϊ�����ӳ�ʼ��һ�� WSAEvent,�� new һ�� SOCKET_OBJ �洢��ȥ
typedef struct _SOCKET_OBJ 
{
	SOCKET m_socket;
	HANDLE m_event;
	sockaddr_in m_remoteAddr;//�ͻ��˵�ַ
	_SOCKET_OBJ *m_pNext;
}SOCKET_OBJ, *PSOCKET_OBJ;



// �����̶߳���thread objects
typedef struct _THREAD_OBJ 
{
	HANDLE m_events[WSA_MAXIMUM_WAIT_EVENTS];//��ǰ�߳�Ҫ�ȴ����¼��������
	int m_socketCount;//��ǰ������׽��ֵ����� <= WSA_MAXIMUM_WAIT_EVENTS
	PSOCKET_OBJ m_head;//�����ͷ
	PSOCKET_OBJ m_tail;
	CRITICAL_SECTION m_cs;//�ؼ�����α���,ͬ���Ըýṹ�Ķ�д
	_THREAD_OBJ *m_pNext;

	_THREAD_OBJ() : m_socketCount(0), m_head(NULL), 
		m_tail(NULL), m_pNext(NULL)
	{
		memset(m_events, 0, sizeof(m_events));
	}
}THREAD_OBJ, *PTHREAD_OBJ;




