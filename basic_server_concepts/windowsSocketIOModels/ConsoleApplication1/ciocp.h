#pragma once

#include "CinitSock.h"

class CIOCPServer;
struct CIOCPBuffer {
	WSAOVERLAPPED m_ol;
	SOCKET m_socketClient; // the client socket AcceptEx
	char *m_buff;
	int m_iLen;
	ULONG m_iSequenceNumber;
	int m_op;
	CIOCPBuffer *m_next;
};

struct CIOCPContext
{
	SOCKET m_socket;
	sockaddr_in m_localAddr;
	sockaddr_in m_remoteAddr;
	bool m_bClosed;
	int m_iOutstandingRecvNumber;
	int m_iOutstandingSendNumber;
	ULONG m_iReadSequence;
	ULONG m_iCurrentReadSequence;
	CIOCPBuffer *pOutOfOrderReads;
	CRITICAL_SECTION m_lock;
	CIOCPContext *m_next;

	void FreeUnfinishedBuffer(CIOCPServer *pServer)
	{
		CIOCPBuffer *pBuffer = pOutOfOrderReads;
		while (pOutOfOrderReads)
		{
			pBuffer = pOutOfOrderReads->m_next;
			pServer->ReleaseBuffer(pOutOfOrderReads);
			pOutOfOrderReads = pBuffer;
		}
	}
};

class CIOCPServer
{
public:
	CIOCPServer();
	~CIOCPServer();

	// start service
	bool Start(const int iPort = 4567, const int iMaxConnections = 2000,
		const int iMaxFreeBuffers = 200, const int iMaxFreeContexts = 100, const int iInitialReads = 4);

	// stop service
	void Shutdown();	
	
	

protected:
	CIOCPBuffer *AllocateBuffer(int iLen);
	void ReleaseBuffer(CIOCPBuffer *pBuffer);
	CIOCPContext *AllocateContext(SOCKET s);
	void ReleaseContext(CIOCPContext *pContext);
	void FreeAllBuffers();
	void FreeAllContexts();

	// buffer 和 context 相关变量定义
	CIOCPContext *m_pFreeContextList;
	CIOCPBuffer *m_pFreeBufferList;
	int m_iFreeBufferCount;
	int m_iFreeContextCount;
	int m_iMaxFreeBuffers;
	int m_iMaxFreeContexts;
	CRITICAL_SECTION m_freeBufferListLock;
	CRITICAL_SECTION m_freeContextListLock;

	// 记录连接列表
	CIOCPContext *m_pConnectionList;
	int m_iCurrentConnectionNumber;
	CRITICAL_SECTION m_connectionListLock;
	int m_iMaxConnectionNumber;
	bool AddAConnection(CIOCPContext *pContext);
	void CloseAConnection(CIOCPContext *pContext);
	void CloseAllConnections();
	inline DWORD GetCurrentConnectionNumber() { return m_iCurrentConnectionNumber; }

	// 记录抛出的接受请求列表
	CIOCPBuffer *m_pendingAccepts;
	long m_iPendingAcceptCount;
	CRITICAL_SECTION m_PendingAcceptsLock;
	void InsertingPendingAccept(CIOCPBuffer *pBuffer);
	bool RemovePendingAccept(CIOCPBuffer *pBuffer);

	// 获得该读的下一个缓冲区对象
	CIOCPBuffer * GetNextCanReadBuffer(CIOCPContext *pContext, CIOCPBuffer *pBUffer);
};