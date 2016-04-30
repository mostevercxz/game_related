#include "ciocp.h"
#include <iostream>
#include <fstream>

SimpleLog::SimpleLog(std::ofstream &of) : m_file(of)
{
	memset(m_buff, 0, sizeof(m_buff));
	::InitializeCriticalSection(&m_critical);
}

SimpleLog::~SimpleLog()
{
	::DeleteCriticalSection(&m_critical);
}

void SimpleLog::Write(const char *fmt, ...)
{
	AutoCritical tmp(&m_critical);
	memset(m_buff, 0, sizeof(m_buff));
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(m_buff, sizeof(m_buff), fmt, ap);
	va_end(ap);
	m_file << m_buff << std::endl;
}

std::ofstream out("debug.txt", std::ofstream::out | std::ofstream::app);
SimpleLog g_log(out);

CIOCPServer::CIOCPServer() : m_pFreeContextList(NULL)
, m_pFreeBufferList(NULL)
, m_iFreeBufferCount(0)
, m_iFreeContextCount(0)
, m_iMaxFreeBuffers(0)
, m_iMaxFreeContexts(0)
, m_pConnectionList(NULL)
, m_iCurrentConnectionNumber(0)
, m_iMaxConnectionNumber(0)
, m_pendingAccepts(NULL)
, m_iPendingAcceptCount(0)
, m_sListen(INVALID_SOCKET)
, m_lpfnAcceptEx(NULL)
, m_iMaxSendNumber(0)
, m_iMaxAcceptsNumber(0)
, m_iPort(bindPort)
, m_initialAccepts(10)
, m_initialReads(4)
, m_shutDown(false)
, m_ioCompletion(NULL)
, m_hAcceptEvent(NULL)
, m_hRepostEvent(NULL)
, m_repostCount(0)
, m_hListenThread(NULL)

{
	::InitializeCriticalSection(&m_freeBufferListLock);
	::InitializeCriticalSection(&m_freeContextListLock);
	::InitializeCriticalSection(&m_connectionListLock);
	::InitializeCriticalSection(&m_PendingAcceptsLock);
	m_hAcceptEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hRepostEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
}

CIOCPServer::~CIOCPServer()
{
	::CloseHandle(m_hAcceptEvent);
	::CloseHandle(m_hRepostEvent);
}

CIOCPBuffer * CIOCPServer::AllocateBuffer(int iLen)
{
	CIOCPBuffer *pBuffer = NULL;
	if (iLen > IOCP_BUFFER_SIZE)
	{
		return NULL;
	}

	{
		AutoCritical tmp(&m_freeBufferListLock);
		if (!m_pFreeBufferList)
		{
			pBuffer = (CIOCPBuffer *)::HeapAlloc(GetProcessHeap(),
				HEAP_ZERO_MEMORY, sizeof(CIOCPBuffer) + IOCP_BUFFER_SIZE);
		}
		else
		{
			pBuffer = m_pFreeBufferList;
			m_pFreeBufferList = m_pFreeBufferList->m_next;
			pBuffer->m_next = NULL;
			--m_iFreeBufferCount;
		}
	}

	// intialize m_buff
	if (pBuffer)
	{
		// pBuffer is CIOCPBuffer, pBuffer + 1 is awesome!!
		pBuffer->m_buff = (char *)(pBuffer + 1);
		pBuffer->m_iLen = iLen;
	}

	return pBuffer;
}

void CIOCPServer::ReleaseBuffer(CIOCPBuffer *pBuffer)
{
	AutoCritical tmp(&m_freeBufferListLock);
	if (m_iFreeBufferCount <= m_iMaxFreeBuffers)
	{
		memset(pBuffer, 0, sizeof(CIOCPBuffer) + IOCP_BUFFER_SIZE);
		pBuffer->m_next = m_pFreeBufferList;
		m_pFreeBufferList = pBuffer;
		++m_iFreeBufferCount;
	}
	else
	{
		::HeapFree(GetProcessHeap(), 0, pBuffer);
	}
}

CIOCPContext *CIOCPServer::AllocateContext(SOCKET s)
{
	CIOCPContext *pContext = NULL;
	{
		AutoCritical tmp(&m_freeContextListLock);
		if (!m_pFreeContextList)
		{
			pContext = (CIOCPContext *)::HeapAlloc(GetProcessHeap(),
				HEAP_ZERO_MEMORY, sizeof(CIOCPContext));
			::InitializeCriticalSection(pContext->m_lock);
		}
		else
		{
			pContext = m_pFreeContextList;
			m_pFreeContextList = m_pFreeContextList->m_next;
			pContext->m_next = NULL;
			--m_iFreeContextCount;
		}
	}

	if (pContext)
	{
		pContext->m_socket = s;
	}

	return pContext;
}

void CIOCPServer::ReleaseContext(CIOCPContext *pContext)
{
	if (pContext->m_socket != INVALID_SOCKET)
	{
		::closesocket(pContext->m_socket);
	}

	// free the unfinished io buffers on this socket
	pContext->FreeUnfinishedBuffer(this);

	AutoCritical tmp(&m_freeContextListLock);
	if (m_iFreeContextCount < m_iMaxFreeContexts)
	{
		CRITICAL_SECTION tmp = pContext->m_lock;
		memset(pContext, 0, sizeof(CIOCPContext));
		pContext->m_lock = tmp;
		pContext->m_next = m_pFreeContextList;
		m_pFreeContextList = pContext;
		++m_iFreeContextCount;
	}
	else
	{
		::DeleteCriticalSection(&pContext->m_lock);
		HeapFree(GetProcessHeap(), 0, pContext);
	}
}

void CIOCPServer::FreeAllBuffers()
{
	// ����m_pFreeBufferList�����б��ͷŻ��������ڴ�
	AutoCritical tmp(&m_freeBufferListLock);


	CIOCPBuffer *pFreeBuffer = m_pFreeBufferList;
	CIOCPBuffer *pNextBuffer;
	while (pFreeBuffer != NULL)
	{
		pNextBuffer = pFreeBuffer->m_next;
		if (!::HeapFree(::GetProcessHeap(), 0, pFreeBuffer))
		{
#ifdef _DEBUG
			::OutputDebugString("  FreeBuffers�ͷ��ڴ����");
#endif // _DEBUG
			break;
		}
		pFreeBuffer = pNextBuffer;
	}
	m_pFreeBufferList = NULL;
	m_iFreeBufferCount = 0;
}

void CIOCPServer::FreeAllContexts()
{
	// ����m_pFreeContextList�����б��ͷŻ��������ڴ�
	AutoCritical tmp(&m_freeContextListLock);

	CIOCPContext *pFreeContext = m_pFreeContextList;
	CIOCPContext *pNextContext;
	while (pFreeContext != NULL)
	{
		pNextContext = pFreeContext->m_next;

		::DeleteCriticalSection(&pFreeContext->m_lock);
		if (!::HeapFree(::GetProcessHeap(), 0, pFreeContext))
		{
#ifdef _DEBUG
			::OutputDebugString("  FreeBuffers�ͷ��ڴ����");
#endif // _DEBUG
			break;
		}
		pFreeContext = pNextContext;
	}
	m_pFreeContextList = NULL;
	m_iFreeContextCount = 0;
}

bool CIOCPServer::AddAConnection(CIOCPContext *pContext)
{
	//��ͻ��������б����һ�� connection
	AutoCritical tmp(&m_connectionListLock);

	if (m_iCurrentConnectionNumber < m_iMaxConnectionNumber)
	{
		pContext->m_next = m_pConnectionList;
		m_pConnectionList = pContext;
		++m_iCurrentConnectionNumber;

		return true;
	}

	return false;
}

void CIOCPServer::CloseAConnection(CIOCPContext *pContext)
{
	// �ȴ� context �б������Ƴ��� context
	{
		AutoCritical tmp(&m_connectionListLock);
		CIOCPContext *pTemp = m_pConnectionList;
		if (pTemp == pContext)
		{
			m_pConnectionList = pContext->m_next;
			--m_iCurrentConnectionNumber;
		}
		else
		{
			while (pTemp && pTemp->m_next != pContext)
			{
				pTemp = pTemp->m_next;
			}
			if (pTemp)
			{
				pTemp->m_next = pContext->m_next;
				--m_iCurrentConnectionNumber;
			}
		}
	}

	// �رտͻ����׽���
	AutoCritical tmp(&pContext->m_lock);
	if (pContext->m_socket != INVALID_SOCKET)
	{
		::closesocket(pContext->m_socket);
		pContext->m_socket = INVALID_SOCKET;
	}
	pContext->m_bClosed = true;
}

void CIOCPServer::CloseAllConnections()
{
	AutoCritical tmp(&m_connectionListLock);

	CIOCPContext *pContext = m_pConnectionList;
	while (pContext)
	{
		// ���Ƴ��׽���
		{
			AutoCritical tmpsocket(&pContext->m_socket);
			if (pContext->m_socket != INVALID_SOCKET)
			{
				::closesocket(pContext->m_socket);
				pContext->m_socket = INVALID_SOCKET;
			}
			pContext->m_bClosed = true;
		}

		pContext = pContext->m_next;
	}

	m_pConnectionList = NULL;
	m_iCurrentConnectionNumber = 0;
}

void CIOCPServer::InsertingPendingAccept(CIOCPBuffer *pBuffer)
{
	// ��һ��IO������������뵽 �б���
	AutoCritical tmp(&m_PendingAcceptsLock);

	if (!m_pendingAccepts)
	{
		m_pendingAccepts = pBuffer;
	}
	else
	{
		pBuffer->m_next = m_pendingAccepts;
		m_pendingAccepts = pBuffer;
	}

	++m_iPendingAcceptCount;
}

bool CIOCPServer::RemovePendingAccept(CIOCPBuffer *pBuffer)
{
	bool bResult = FALSE;

	// ����m_pPendingAccepts�������Ƴ�pBuffer��ָ��Ļ���������
	AutoCritical tmp(&m_PendingAcceptsLock);

	CIOCPBuffer *pTest = m_pConnectionList;
	if (pTest == pBuffer)	// ����Ǳ�ͷԪ��
	{
		m_pConnectionList = pBuffer->pNext;
		bResult = TRUE;
	}
	else					// ���Ǳ�ͷԪ�صĻ�����Ҫ�����������������
	{
		while (pTest != NULL && pTest->pNext != pBuffer)
			pTest = pTest->pNext;
		if (pTest != NULL)
		{
			pTest->pNext = pBuffer->pNext;
			bResult = TRUE;
		}
	}
	// ���¼���
	if (bResult)
		--m_iPendingAcceptCount;	

	return  bResult;
}


// pBuffer �Ǹ��յ��Ķ�������ɻ�����
CIOCPBuffer * CIOCPServer::GetNextCanReadBuffer(CIOCPContext *pContext, CIOCPBuffer *pBUffer)
{
	if (pBUffer)
	{
		if (pBUffer->m_iSequenceNumber == pContext->m_iCurrentReadSequence)
		{
			g_log.Write("pBuffer����pContext��һ��Ҫ�������к�,Ϊ%d", pBUffer->m_iSequenceNumber);
			return pBUffer;
		}

		// ��������,��˵��û�а�˳���������,����黺�������浽 pContext �� pOutOfOrderReads �б���
		// ע���б��еĻ������ǰ������кŴ�С�������е�,��ʵ����ֱ������map�ġ����α���ô�鷳
		pBUffer->m_next = NULL;
		CIOCPBuffer *ptr = pContext->pOutOfOrderReads;
		CIOCPBuffer *pPre = NULL;
		while (ptr)
		{
			if (pBUffer->m_iSequenceNumber < ptr->m_iSequenceNumber)
			{
				break;
			}

			pPre = ptr;
			ptr = ptr->m_next;
		}

		if (!pPre)
		{
			pBUffer->m_next = pContext->pOutOfOrderReads;
			pContext->pOutOfOrderReads = pBUffer;
		}
		else
		{			
			pBUffer->m_next = pPre->m_next;
			pPre->m_next = pBUffer;
		}
	}

	CIOCPBuffer *ptr = pContext->pOutOfOrderReads;
	if (ptr && ptr->m_iSequenceNumber == pContext->m_iReadSequence)
	{
		g_log.Write("pContext��ͷbuffer �� pContext��һ��Ҫ�������к�һ��,���кŶ�Ϊ%d", ptr->m_iSequenceNumber);
		pContext->pOutOfOrderReads = ptr->m_next;
		return ptr;
	}

	return NULL;
}

bool CIOCPServer::PostAccept(CIOCPBuffer *pBuffer)
{
	pBuffer->m_op = OP_ACCEPT;

	DWORD dwBytes = 0;
	pBuffer->m_socketClient = ::WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	bool b = m_lpfnAcceptEx(m_sListen,
		pBuffer->m_socketClient,
		pBuffer->m_buff,
		pBuffer->m_iLen - ((sizeof(sockaddr_in) + 16) * 2),
		sizeof(sockaddr_in) + 16,
		sizeof(sockaddr_in) + 16,
		&dwBytes,
		&pBuffer->m_ol);

	int errorCode = ::WSAGetLastError();
	if (!b && errorCode != WSA_IO_PENDING)
	{
		g_log.Write("PostAccept()��������,error code=%d", errorCode);
		return false;
	}

	return true;
}

bool CIOCPServer::PostRecv(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{
	pBuffer->m_op = OP_READ;

	AutoCritical tmp(&pContext->m_lock);

	// �������к�
	pBuffer->m_iSequenceNumber = pContext->m_iReadSequence;
	// Ͷ�ݴ��ص�IO
	DWORD dwBytes;
	DWORD dwFlags = 0;
	WSABUF buf;
	buf.buf = pBuffer->m_buff;
	buf.len = pBuffer->m_iLen;
	if (::WSARecv(pContext->m_socket, &buf, 1, &dwBytes, &dwFlags, &pBuffer->m_ol, NULL) != NO_ERROR)
	{
		int errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			g_log.Write("PostRecv() ,WSARecv() error,code=%d", errorCode);
			return false;
		}
	}

	// �����׽����ϵ��ص�IO����,�Ͷ����кż���
	++pContext->m_iOutstandingRecvNumber;
	++pContext->m_iReadSequence;
	return true;
}

bool CIOCPServer::PostSend(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{
	// ����Ͷ�ݵķ�������
	if (pContext->m_iOutstandingSendNumber > m_iMaxSendNumber)
	{
		g_log.Write("context �ķ������� %d �������ɷ������� %d",
			pContext->m_iOutstandingSendNumber, m_iMaxSendNumber);
		return false;
	}

	// Ͷ�ݴ��ص�IO
	pBuffer->m_op = OP_WRITE;
	DWORD dwBytes;
	DWORD dwFlags = 0;
	WSABUF buf;
	buf.buf = pBuffer->m_buff;
	buf.len = pBuffer->m_iLen;
	if (::WSASend(pContext->m_socket, &buf, 1, &dwBytes, dwFlags, &pBuffer->m_ol, NULL) != NO_ERROR)
	{
		int errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			g_log.Write("PostSend(),::WSASend() error,code=%d", errorCode);
			return false;
		}
	}

	AutoCritical tmp(&pContext->m_lock);
	++pContext->m_iOutstandingSendNumber;

	return true;
}

bool CIOCPServer::Start(int nPort, int nMaxConnections, int nMaxFreeBuffers, int nMaxFreeContexts, int nInitialReads)
{
	m_iPort = nPort;
	m_iMaxConnectionNumber = nMaxConnections;
	m_iMaxFreeBuffers = nMaxFreeBuffers;
	m_iFreeContextCount = nMaxFreeContexts;
	m_initialReads = nInitialReads;

	m_sListen = ::WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN si;
	si.sin_family = AF_INET;
	si.sin_port = ::ntohs(m_iPort);
	si.sin_addr.S_un.S_addr = INADDR_ANY;
	if (::bind(m_sListen, (sockaddr*)&si, sizeof(si)) == SOCKET_ERROR)
	{
		g_log.Write("����������ʧ��,bind()ʧ��");
		return false;
	}
	::listen(m_sListen, 200);

	m_ioCompletion = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);

	// ������չ����AcceptEx
	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	DWORD dwBytes;
	::WSAIoctl(m_sListen,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx,
		sizeof(GuidAcceptEx),
		&m_lpfnAcceptEx,
		sizeof(m_lpfnAcceptEx),
		&dwBytes,
		NULL,
		NULL);

	// ������չ����GetAcceptExSockaddrs
	GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
	::WSAIoctl(m_sListen,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidGetAcceptExSockaddrs,
		sizeof(GuidGetAcceptExSockaddrs),
		&m_lpfnGetAcceptExSockaddrs,
		sizeof(m_lpfnGetAcceptExSockaddrs),
		&dwBytes,
		NULL,
		NULL
	);

	// �������׽��ֹ�������ɶ˿ڣ�ע�⣬����Ϊ�����ݵ�CompletionKeyΪ0
	::CreateIoCompletionPort((HANDLE)m_sListen, m_ioCompletion, (DWORD)0, 0);

	// ע��FD_ACCEPT�¼���
	// ���Ͷ�ݵ�AcceptEx I/O�������̻߳���յ�FD_ACCEPT�����¼���˵��Ӧ��Ͷ�ݸ����AcceptEx I/O
	WSAEventSelect(m_sListen, m_hAcceptEvent, FD_ACCEPT);

	// ���������߳�
	m_hListenThread = ::CreateThread(NULL, 0, _ListenThreadProc, this, 0, NULL);

	return true;
}

void CIOCPServer::Shutdown()
{
	m_shutDown = true;
	::SetEvent(m_hAcceptEvent);
	// �ȴ������߳��˳�
	::WaitForSingleObject(m_hListenThread, INFINITE);
	::CloseHandle(m_hListenThread);
	m_hListenThread = NULL;	
}

// �ڼ����׽�����Ͷ�� AcceptEx IO ����
/*
1. �����ʼ��,ҪͶ�ݼ��� accept ����
2. ���� IO ���߳��յ�һ���ͻ�, ʹ m_hRepostEvent ����, _ListenThreadProc �յ�֪ͨ����Ͷ��һ�� Accept ����
3. ���������ڼ�,Ͷ�ݵ�Accept ���󲻹���,�û�����������δ�ܹ����ϴ���, m_hAcceptEvent �¼�����ͻ�����,��ʱ��Ͷ�����ɸ� Accept ����
*/
DWORD WINAPI CIOCPServer::_ListenThreadProc(LPVOID lpParam)
{
	CIOCPServer *pThis = (CIOCPServer*)lpParam;

	// ���ڼ����׽�����Ͷ�ݼ���Accept I/O
	CIOCPBuffer *pBuffer;
	for (int i = 0; i < pThis->m_initialAccepts; i++)
	{
		pBuffer = pThis->AllocateBuffer(IOCP_BUFFER_SIZE);
		if (pBuffer == NULL)
			return -1;
		pThis->InsertingPendingAccept(pBuffer);
		pThis->PostAccept(pBuffer);
	}

	// �����¼��������飬�Ա����������WSAWaitForMultipleEvents����
	HANDLE hWaitEvents[2 + IOCP_THREAD_NUM];
	int nEventCount = 0;
	hWaitEvents[nEventCount++] = pThis->m_hAcceptEvent;
	hWaitEvents[nEventCount++] = pThis->m_hRepostEvent;

	// ����ָ�������Ĺ����߳�����ɶ˿��ϴ���I/O
	for (int i = 0; i < IOCP_THREAD_NUM; i++)
	{
		hWaitEvents[nEventCount++] = ::CreateThread(NULL, 0, _WorkerThreadProc, pThis, 0, NULL);
	}

	while (true)
	{
		int iIndex = ::WSAWaitForMultipleEvents(nEventCount, hWaitEvents, FALSE, 60 * 1000, false);
		if (iIndex == WSA_WAIT_FAILED)
		{
			// �ر���������
			pThis->CloseAllConnections();
			::Sleep(0);		// ��I/O�����߳�һ��ִ�еĻ���
							// �رռ����׽���
			::closesocket(pThis->m_sListen);
			pThis->m_sListen = INVALID_SOCKET;
			::Sleep(0);		// ��I/O�����߳�һ��ִ�еĻ���

							// ֪ͨ����I/O�����߳��˳�
			for (int i = 2; i < IOCP_THREAD_NUM  + 2; i++)
			{
				::PostQueuedCompletionStatus(pThis->m_ioCompletion, -1, 0, NULL);
			}

			// �ȴ�I/O�����߳��˳�
			::WaitForMultipleObjects(IOCP_THREAD_NUM, &hWaitEvents[2], TRUE, 5 * 1000);

			for (int i = 2; i < IOCP_THREAD_NUM + 2; i++)
			{
				::CloseHandle(hWaitEvents[i]);
			}

			::CloseHandle(pThis->m_ioCompletion);

			pThis->FreeAllBuffers();
			pThis->FreeAllContexts();
			::ExitThread(0);
		}

		// 1����ʱ�������δ���ص�AcceptEx I/O�����ӽ����˶೤ʱ��
		if (iIndex == WSA_WAIT_TIMEOUT)
		{
			pBuffer = pThis->m_pendingAccepts;
			while (pBuffer != NULL)
			{
				int nSeconds;
				int nLen = sizeof(nSeconds);
				// ȡ�����ӽ�����ʱ��
				::getsockopt(pBuffer->m_socketClient,
					SOL_SOCKET, SO_CONNECT_TIME, (char *)&nSeconds, &nLen);
				// �������2���ӿͻ��������ͳ�ʼ���ݣ���������ͻ�go away
				if (nSeconds != -1 && nSeconds > 2 * 60)
				{
					closesocket(pBuffer->m_socketClient);
					pBuffer->m_socketClient = INVALID_SOCKET;
				}

				pBuffer = pBuffer->m_next;
			}
		}
		else
		{
			iIndex = iIndex - WAIT_OBJECT_0;
			WSANETWORKEVENTS ne;
			int nLimit = 0;
			if (iIndex == 0)			// 2��m_hAcceptEvent�¼��������ţ�˵��Ͷ�ݵ�Accept���󲻹�����Ҫ����
			{
				::WSAEnumNetworkEvents(pThis->m_sListen, hWaitEvents[iIndex], &ne);
				if (ne.lNetworkEvents & FD_ACCEPT)
				{
					nLimit = 50;  // ���ӵĸ�����������Ϊ50��
				}
			}
			else if (iIndex == 1)	// 3��m_hRepostEvent�¼��������ţ�˵������I/O���߳̽��ܵ��µĿͻ�
			{
				nLimit = InterlockedExchange(&pThis->m_repostCount, 0);
			}
			else if (iIndex > 1)		// I/O�����߳��˳���˵���д��������رշ�����
			{
				pThis->m_shutDown = true;
				continue;
			}

			// Ͷ��nLimit��AcceptEx I/O����
			int i = 0;
			while (i++ < nLimit && pThis->m_iPendingAcceptCount < pThis->m_iMaxAcceptsNumber)
			{
				pBuffer = pThis->AllocateBuffer(IOCP_BUFFER_SIZE);
				if (pBuffer != NULL)
				{
					pThis->InsertingPendingAccept(pBuffer);
					pThis->PostAccept(pBuffer);
				}
			}
		}
	}

	return 0;
}

DWORD WINAPI CIOCPServer::_WorkerThreadProc(LPVOID lpParam)
{
#ifdef _DEBUG
	::OutputDebugString("	WorkerThread ����... \n");
#endif // _DEBUG

	CIOCPServer *pThis = (CIOCPServer*)lpParam;

	CIOCPBuffer *pBuffer;
	ULONG dwKey;
	DWORD dwTrans;
	LPOVERLAPPED lpol;
	while (TRUE)
	{
		// �ڹ���������ɶ˿ڵ������׽����ϵȴ�I/O���
		BOOL bOK = ::GetQueuedCompletionStatus(pThis->m_ioCompletion,
			&dwTrans, (ULONG_PTR*)&dwKey, (LPOVERLAPPED*)&lpol, WSA_INFINITE);

		if (dwTrans == -1) // �û�֪ͨ�˳�
		{
#ifdef _DEBUG
			::OutputDebugString("	WorkerThread �˳� \n");
#endif // _DEBUG
			::ExitThread(0);
		}

		pBuffer = CONTAINING_RECORD(lpol, CIOCPBuffer, m_ol);
		int nError = NO_ERROR;
		if (!bOK)						// �ڴ��׽������д�����
		{
			SOCKET s;
			if (pBuffer->m_op == OP_ACCEPT)
			{
				s = pThis->m_sListen;
			}
			else
			{
				if (dwKey == 0)
					break;
				s = ((CIOCPContext*)dwKey)->m_socket;
			}
			DWORD dwFlags = 0;
			if (!::WSAGetOverlappedResult(s, &pBuffer->m_ol, &dwTrans, FALSE, &dwFlags))
			{
				nError = ::WSAGetLastError();
			}
		}
		pThis->HandleIO(dwKey, pBuffer, dwTrans, nError);
	}

#ifdef _DEBUG
	::OutputDebugString("	WorkerThread �˳� \n");
#endif // _DEBUG
	return 0;
}


bool CIOCPServer::SendText(CIOCPContext *pContext, char *pszText, int nLen)
{
	CIOCPBuffer *pBuffer = AllocateBuffer(nLen);
	if (pBuffer != NULL)
	{
		memcpy(pBuffer->m_buff, pszText, nLen);
		return PostSend(pContext, pBuffer);
	}
	return FALSE;
}

void CIOCPServer::HandleIO(DWORD dwKey, CIOCPBuffer *pBuffer, DWORD dwTrans, int nError)
{
	CIOCPContext *pContext = (CIOCPContext *)dwKey;

#ifdef _DEBUG
	::OutputDebugString("	HandleIO... \n");
#endif // _DEBUG

	// 1�����ȼ����׽����ϵ�δ��I/O����
	if (pContext != NULL)
	{
		{
			AutoCritical tmp(&pContext->Lock);

			if (pBuffer->m_op == OP_READ)
			{
				--pContext->m_iOutstandingRecvNumber;
			}
			else if (pBuffer->m_op == OP_WRITE)
			{
				--pContext->m_iOutstandingSendNumber;
			}
		}		

		// 2������׽����Ƿ��Ѿ������ǹر�
		if (pContext->m_bClosed)
		{
#ifdef _DEBUG
			::OutputDebugString("	��鵽�׽����Ѿ������ǹر� \n");
#endif // _DEBUG
			if (pContext->m_iOutstandingRecvNumber == 0 && pContext->m_iOutstandingSendNumber == 0)
			{
				ReleaseContext(pContext);
			}
			// �ͷ��ѹر��׽��ֵ�δ��I/O
			ReleaseBuffer(pBuffer);
			return;
		}
	}
	else
	{
		RemovePendingAccept(pBuffer);
	}

	// 3������׽����Ϸ����Ĵ�������еĻ���֪ͨ�û���Ȼ��ر��׽���
	if (nError != NO_ERROR)
	{
		if (pBuffer->m_op != OP_ACCEPT)
		{
			OnConnectionError(pContext, pBuffer, nError);
			CloseAConnection(pContext);
			if (pContext->m_iOutstandingRecvNumber == 0 && pContext->m_iOutstandingSendNumber == 0)
			{
				ReleaseContext(pContext);
			}
#ifdef _DEBUG
			::OutputDebugString("	��鵽�ͻ��׽����Ϸ������� \n");
#endif // _DEBUG
		}
		else // �ڼ����׽����Ϸ�������Ҳ���Ǽ����׽��ִ���Ŀͻ�������
		{
			// �ͻ��˳����ͷ�I/O������
			if (pBuffer->m_socketClient != INVALID_SOCKET)
			{
				::closesocket(pBuffer->m_socketClient);
				pBuffer->m_socketClient = INVALID_SOCKET;
			}
#ifdef _DEBUG
			::OutputDebugString("	��鵽�����׽����Ϸ������� \n");
#endif // _DEBUG
		}

		ReleaseBuffer(pBuffer);
		return;
	}


	// ��ʼ����
	if (pBuffer->m_op == OP_ACCEPT)
	{
		if (dwTrans == 0)
		{
#ifdef _DEBUG
			::OutputDebugString("	�����׽����Ͽͻ��˹ر� \n");
#endif // _DEBUG

			if (pBuffer->sClim_socketClientent != INVALID_SOCKET)
			{
				::closesocket(pBuffer->m_socketClient);
				pBuffer->m_socketClient = INVALID_SOCKET;
			}
		}
		else
		{
			// Ϊ�½��ܵ���������ͻ������Ķ���
			CIOCPContext *pClient = AllocateContext(pBuffer->m_socketClient);
			if (pClient != NULL)
			{
				if (AddAConnection(pClient))
				{
					// ȡ�ÿͻ���ַ
					int nLocalLen, nRmoteLen;
					LPSOCKADDR pLocalAddr, pRemoteAddr;
					m_lpfnGetAcceptExSockaddrs(
						pBuffer->m_buff,
						pBuffer->m_iLen - ((sizeof(sockaddr_in) + 16) * 2),
						sizeof(sockaddr_in) + 16,
						sizeof(sockaddr_in) + 16,
						(SOCKADDR **)&pLocalAddr,
						&nLocalLen,
						(SOCKADDR **)&pRemoteAddr,
						&nRmoteLen);
					memcpy(&pClient->m_localAddr, pLocalAddr, nLocalLen);
					memcpy(&pClient->m_remoteAddr, pRemoteAddr, nRmoteLen);

					// ���������ӵ���ɶ˿ڶ���
					::CreateIoCompletionPort((HANDLE)pClient->m_socket, m_ioCompletion, (DWORD)pClient, 0);

					// ֪ͨ�û�
					pBuffer->m_iLen = dwTrans;
					OnConnectionEstablished(pClient, pBuffer);

					// ��������Ͷ�ݼ���Read������Щ�ռ����׽��ֹرջ����ʱ�ͷ�
					for (int i = 0; i < 5; i++)
					{
						CIOCPBuffer *p = AllocateBuffer(IOCP_BUFFER_SIZE);
						if (p != NULL)
						{
							if (!PostRecv(pClient, p))
							{
								CloseAConnection(pClient);
								break;
							}
						}
					}
				}
				else	// ���������������ر�����
				{
					CloseAConnection(pClient);
					ReleaseContext(pClient);
				}
			}
			else
			{
				// ��Դ���㣬�ر���ͻ������Ӽ���
				::closesocket(pBuffer->m_socketClient);
				pBuffer->m_socketClient = INVALID_SOCKET;
			}
		}

		// Accept������ɣ��ͷ�I/O������
		ReleaseBuffer(pBuffer);

		// ֪ͨ�����̼߳�����Ͷ��һ��Accept����
		::InterlockedIncrement(&m_repostCount);
		::SetEvent(m_hRepostEvent);
	}
	else if (pBuffer->m_op == OP_READ)
	{
		if (dwTrans == 0)	// �Է��ر��׽���
		{
			// ��֪ͨ�û�
			pBuffer->m_iLen = 0;
			OnConnectionClosing(pContext, pBuffer);
			// �ٹر�����
			CloseAConnection(pContext);
			// �ͷſͻ������ĺͻ���������
			if (pContext->m_iOutstandingRecvNumber == 0 && pContext->m_iOutstandingSendNumber == 0)
			{
				ReleaseContext(pContext);
			}
			ReleaseBuffer(pBuffer);
		}
		else
		{
			pBuffer->nLen = dwTrans;
			// ����I/OͶ�ݵ�˳���ȡ���յ�������
			CIOCPBuffer *p = GetNextCanReadBuffer(pContext, pBuffer);
			while (p != NULL)
			{
				// ֪ͨ�û�
				OnReadCompleted(pContext, p);
				// ����Ҫ�������кŵ�ֵ
				::InterlockedIncrement((LONG*)&pContext->m_iCurrentReadSequence);
				// �ͷ��������ɵ�I/O
				ReleaseBuffer(p);
				p = GetNextCanReadBuffer(pContext, NULL);
			}

			// ����Ͷ��һ���µĽ�������
			pBuffer = AllocateBuffer(IOCP_BUFFER_SIZE);
			if (pBuffer == NULL || !PostRecv(pContext, pBuffer))
			{
				CloseAConnection(pContext);
			}
		}
	}
	else if (pBuffer->m_op == OP_WRITE)
	{

		if (dwTrans == 0)	// �Է��ر��׽���
		{
			// ��֪ͨ�û�
			pBuffer->m_iLen = 0;
			OnConnectionClosing(pContext, pBuffer);

			// �ٹر�����
			CloseAConnection(pContext);

			// �ͷſͻ������ĺͻ���������
			if (pContext->m_iOutstandingRecvNumber == 0 && pContext->m_iOutstandingSendNumber == 0)
			{
				ReleaseContext(pContext);
			}
			ReleaseBuffer(pBuffer);
		}
		else
		{
			// д������ɣ�֪ͨ�û�
			pBuffer->m_iLen = dwTrans;
			OnWriteCompleted(pContext, pBuffer);
			// �ͷ�SendText��������Ļ�����
			ReleaseBuffer(pBuffer);
		}
	}
}

void CIOCPServer::OnConnectionEstablished(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{
	g_log.Write("OnConnectionEstablished");
}

void CIOCPServer::OnConnectionClosing(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{
	g_log.Write("OnConnectionClosing");
}


void CIOCPServer::OnReadCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{
	g_log.Write("OnReadCompleted");
}

void CIOCPServer::OnWriteCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{
	g_log.Write("OnWriteCompleted");
}

void CIOCPServer::OnConnectionError(CIOCPContext *pContext, CIOCPBuffer *pBuffer, int nError)
{
	g_log.Write("OnConnectionError, errorCode=%d", nError);
}
