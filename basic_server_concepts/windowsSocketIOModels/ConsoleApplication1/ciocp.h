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
	int m_iOutstandingRecvNumber;//δ������IO����
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

	// buffer �� context ��ر�������
	CIOCPContext *m_pFreeContextList;
	CIOCPBuffer *m_pFreeBufferList;
	int m_iFreeBufferCount;
	int m_iFreeContextCount;
	int m_iMaxFreeBuffers;
	int m_iMaxFreeContexts;
	CRITICAL_SECTION m_freeBufferListLock;
	CRITICAL_SECTION m_freeContextListLock;

	// ��¼�����б�
	CIOCPContext *m_pConnectionList;
	int m_iCurrentConnectionNumber;
	CRITICAL_SECTION m_connectionListLock;
	int m_iMaxConnectionNumber;
	bool AddAConnection(CIOCPContext *pContext);
	void CloseAConnection(CIOCPContext *pContext);
	void CloseAllConnections();
	inline DWORD GetCurrentConnectionNumber() { return m_iCurrentConnectionNumber; }

	// ��¼�׳��Ľ��������б�
	CIOCPBuffer *m_pendingAccepts;
	long m_iPendingAcceptCount;
	CRITICAL_SECTION m_PendingAcceptsLock;
	void InsertingPendingAccept(CIOCPBuffer *pBuffer);
	bool RemovePendingAccept(CIOCPBuffer *pBuffer);

	// ��øö�����һ������������
	CIOCPBuffer * GetNextCanReadBuffer(CIOCPContext *pContext, CIOCPBuffer *pBUffer);

	// Ͷ���ص�IO
	SOCKET m_sListen;//�����׽��־��
	LPFN_ACCEPTEX m_lpfnAcceptEx;//AcceptEx ������ַ
	int m_iMaxSendNumber;//����͵�����
	int m_iMaxAcceptsNumber;
	bool PostAccept(CIOCPBuffer *pBuffer);
	bool PostRecv(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	bool PostSend(CIOCPContext *pContext, CIOCPBuffer *pBuffer);

	// ��ʼֹͣ����
	int m_iPort;
	int m_initialAccepts;
	int m_initialReads;	
	bool m_shutDown;//֪ͨ�����߳�ֹͣ����
	bool Start(int nPort = 4567, int nMaxConnections = 2000,
		int nMaxFreeBuffers = 200, int nMaxFreeContexts = 100, int nInitialReads = 4);
	void Shutdown();

	// һЩ handle
	HANDLE m_ioCompletion;
	HANDLE m_hAcceptEvent;//�����¼�
	HANDLE m_hRepostEvent;//repost �¼�
	LONG m_repostCount;
	HANDLE m_hListenThread;
	LPFN_GETACCEPTEXSOCKADDRS m_lpfnGetAcceptExSockaddrs; // GetAcceptExSockaddrs������ַ
	void HandleIO(DWORD dwKey, CIOCPBuffer *pBuffer, DWORD dwTrans, int nError);
	bool SendText(CIOCPContext *pContext, char *pszText, int nLen);

	// �¼�֪ͨ����
	// ������һ���µ�����
	virtual void OnConnectionEstablished(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	// һ�����ӹر�
	virtual void OnConnectionClosing(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	// ��һ�������Ϸ����˴���
	virtual void OnConnectionError(CIOCPContext *pContext, CIOCPBuffer *pBuffer, int nError);
	// һ�������ϵĶ��������
	virtual void OnReadCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	// һ�������ϵ�д�������
	virtual void OnWriteCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer);

private:
	static DWORD WINAPI _ListenThreadProc(LPVOID lpParam);
	static DWORD WINAPI _WorkerThreadProc(LPVOID lpParam);
};