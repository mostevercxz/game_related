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
	int m_iOutstandingRecvNumber;//未决接收IO计数
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

	// 投递重叠IO
	SOCKET m_sListen;//监听套接字句柄
	LPFN_ACCEPTEX m_lpfnAcceptEx;//AcceptEx 函数地址
	int m_iMaxSendNumber;//最大发送的数量
	int m_iMaxAcceptsNumber;
	bool PostAccept(CIOCPBuffer *pBuffer);
	bool PostRecv(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	bool PostSend(CIOCPContext *pContext, CIOCPBuffer *pBuffer);

	// 开始停止服务
	int m_iPort;
	int m_initialAccepts;
	int m_initialReads;	
	bool m_shutDown;//通知监听线程停止服务
	bool Start(int nPort = 4567, int nMaxConnections = 2000,
		int nMaxFreeBuffers = 200, int nMaxFreeContexts = 100, int nInitialReads = 4);
	void Shutdown();

	// 一些 handle
	HANDLE m_ioCompletion;
	HANDLE m_hAcceptEvent;//接收事件
	HANDLE m_hRepostEvent;//repost 事件
	LONG m_repostCount;
	HANDLE m_hListenThread;
	LPFN_GETACCEPTEXSOCKADDRS m_lpfnGetAcceptExSockaddrs; // GetAcceptExSockaddrs函数地址
	void HandleIO(DWORD dwKey, CIOCPBuffer *pBuffer, DWORD dwTrans, int nError);
	bool SendText(CIOCPContext *pContext, char *pszText, int nLen);

	// 事件通知函数
	// 建立了一个新的连接
	virtual void OnConnectionEstablished(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	// 一个连接关闭
	virtual void OnConnectionClosing(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	// 在一个连接上发生了错误
	virtual void OnConnectionError(CIOCPContext *pContext, CIOCPBuffer *pBuffer, int nError);
	// 一个连接上的读操作完成
	virtual void OnReadCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	// 一个连接上的写操作完成
	virtual void OnWriteCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer);

private:
	static DWORD WINAPI _ListenThreadProc(LPVOID lpParam);
	static DWORD WINAPI _WorkerThreadProc(LPVOID lpParam);
};