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
	// 遍历m_pFreeBufferList空闲列表，释放缓冲区池内存
	AutoCritical tmp(&m_freeBufferListLock);


	CIOCPBuffer *pFreeBuffer = m_pFreeBufferList;
	CIOCPBuffer *pNextBuffer;
	while (pFreeBuffer != NULL)
	{
		pNextBuffer = pFreeBuffer->m_next;
		if (!::HeapFree(::GetProcessHeap(), 0, pFreeBuffer))
		{
#ifdef _DEBUG
			::OutputDebugString("  FreeBuffers释放内存出错！");
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
	// 遍历m_pFreeContextList空闲列表，释放缓冲区池内存
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
			::OutputDebugString("  FreeBuffers释放内存出错！");
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
	//向客户端连接列表添加一个 connection
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
	// 先从 context 列表里面移除该 context
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

	// 关闭客户端套接字
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
		// 先移除套接字
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
	// 将一个IO缓冲区对象插入到 列表中
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

	// 遍历m_pPendingAccepts表，从中移除pBuffer所指向的缓冲区对象
	AutoCritical tmp(&m_PendingAcceptsLock);

	CIOCPBuffer *pTest = m_pConnectionList;
	if (pTest == pBuffer)	// 如果是表头元素
	{
		m_pConnectionList = pBuffer->pNext;
		bResult = TRUE;
	}
	else					// 不是表头元素的话，就要遍历这个表来查找了
	{
		while (pTest != NULL && pTest->pNext != pBuffer)
			pTest = pTest->pNext;
		if (pTest != NULL)
		{
			pTest->pNext = pBuffer->pNext;
			bResult = TRUE;
		}
	}
	// 更新计数
	if (bResult)
		--m_iPendingAcceptCount;	

	return  bResult;
}


// pBuffer 是刚收到的读操作完成缓冲区
CIOCPBuffer * CIOCPServer::GetNextCanReadBuffer(CIOCPContext *pContext, CIOCPBuffer *pBUffer)
{
	if (pBUffer)
	{
		if (pBUffer->m_iSequenceNumber == pContext->m_iCurrentReadSequence)
		{
			g_log.Write("pBuffer就是pContext下一个要读的序列号,为%d", pBUffer->m_iSequenceNumber);
			return pBUffer;
		}

		// 如果不相等,则说明没有按顺序接受数据,将这块缓冲区保存到 pContext 的 pOutOfOrderReads 列表中
		// 注意列表中的缓冲区是按照序列号从小到大排列的,其实可以直接做个map的。。何必这么麻烦
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
		g_log.Write("pContext表头buffer 和 pContext下一个要读的序列号一致,序列号都为%d", ptr->m_iSequenceNumber);
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
		g_log.Write("PostAccept()发生错误,error code=%d", errorCode);
		return false;
	}

	return true;
}

bool CIOCPServer::PostRecv(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{
	pBuffer->m_op = OP_READ;

	AutoCritical tmp(&pContext->m_lock);

	// 设置序列号
	pBuffer->m_iSequenceNumber = pContext->m_iReadSequence;
	// 投递此重叠IO
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

	// 增加套接字上的重叠IO计数,和读序列号计数
	++pContext->m_iOutstandingRecvNumber;
	++pContext->m_iReadSequence;
	return true;
}

bool CIOCPServer::PostSend(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{
	// 跟踪投递的发送数量
	if (pContext->m_iOutstandingSendNumber > m_iMaxSendNumber)
	{
		g_log.Write("context 的发送数量 %d 大于最大可发送数量 %d",
			pContext->m_iOutstandingSendNumber, m_iMaxSendNumber);
		return false;
	}

	// 投递此重叠IO
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
		g_log.Write("服务器启动失败,bind()失败");
		return false;
	}
	::listen(m_sListen, 200);

	m_ioCompletion = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);

	// 加载扩展函数AcceptEx
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

	// 加载扩展函数GetAcceptExSockaddrs
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

	// 将监听套节字关联到完成端口，注意，这里为它传递的CompletionKey为0
	::CreateIoCompletionPort((HANDLE)m_sListen, m_ioCompletion, (DWORD)0, 0);

	// 注册FD_ACCEPT事件。
	// 如果投递的AcceptEx I/O不够，线程会接收到FD_ACCEPT网络事件，说明应该投递更多的AcceptEx I/O
	WSAEventSelect(m_sListen, m_hAcceptEvent, FD_ACCEPT);

	// 创建监听线程
	m_hListenThread = ::CreateThread(NULL, 0, _ListenThreadProc, this, 0, NULL);

	return true;
}

void CIOCPServer::Shutdown()
{
	m_shutDown = true;
	::SetEvent(m_hAcceptEvent);
	// 等待监听线程退出
	::WaitForSingleObject(m_hListenThread, INFINITE);
	::CloseHandle(m_hListenThread);
	m_hListenThread = NULL;	
}

// 在监听套接字上投递 AcceptEx IO 请求
/*
1. 程序初始化,要投递几个 accept 请求
2. 处理 IO 的线程收到一个客户, 使 m_hRepostEvent 受信, _ListenThreadProc 收到通知后再投递一个 Accept 请求
3. 程序运行期间,投递的Accept 请求不够用,用户的连接请求未能够马上处理, m_hAcceptEvent 事件对象就会受信,此时再投递若干个 Accept 请求
*/
DWORD WINAPI CIOCPServer::_ListenThreadProc(LPVOID lpParam)
{
	CIOCPServer *pThis = (CIOCPServer*)lpParam;

	// 先在监听套节字上投递几个Accept I/O
	CIOCPBuffer *pBuffer;
	for (int i = 0; i < pThis->m_initialAccepts; i++)
	{
		pBuffer = pThis->AllocateBuffer(IOCP_BUFFER_SIZE);
		if (pBuffer == NULL)
			return -1;
		pThis->InsertingPendingAccept(pBuffer);
		pThis->PostAccept(pBuffer);
	}

	// 构建事件对象数组，以便在上面调用WSAWaitForMultipleEvents函数
	HANDLE hWaitEvents[2 + IOCP_THREAD_NUM];
	int nEventCount = 0;
	hWaitEvents[nEventCount++] = pThis->m_hAcceptEvent;
	hWaitEvents[nEventCount++] = pThis->m_hRepostEvent;

	// 创建指定数量的工作线程在完成端口上处理I/O
	for (int i = 0; i < IOCP_THREAD_NUM; i++)
	{
		hWaitEvents[nEventCount++] = ::CreateThread(NULL, 0, _WorkerThreadProc, pThis, 0, NULL);
	}

	while (true)
	{
		int iIndex = ::WSAWaitForMultipleEvents(nEventCount, hWaitEvents, FALSE, 60 * 1000, false);
		if (iIndex == WSA_WAIT_FAILED)
		{
			// 关闭所有连接
			pThis->CloseAllConnections();
			::Sleep(0);		// 给I/O工作线程一个执行的机会
							// 关闭监听套节字
			::closesocket(pThis->m_sListen);
			pThis->m_sListen = INVALID_SOCKET;
			::Sleep(0);		// 给I/O工作线程一个执行的机会

							// 通知所有I/O处理线程退出
			for (int i = 2; i < IOCP_THREAD_NUM  + 2; i++)
			{
				::PostQueuedCompletionStatus(pThis->m_ioCompletion, -1, 0, NULL);
			}

			// 等待I/O处理线程退出
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

		// 1）定时检查所有未返回的AcceptEx I/O的连接建立了多长时间
		if (iIndex == WSA_WAIT_TIMEOUT)
		{
			pBuffer = pThis->m_pendingAccepts;
			while (pBuffer != NULL)
			{
				int nSeconds;
				int nLen = sizeof(nSeconds);
				// 取得连接建立的时间
				::getsockopt(pBuffer->m_socketClient,
					SOL_SOCKET, SO_CONNECT_TIME, (char *)&nSeconds, &nLen);
				// 如果超过2分钟客户还不发送初始数据，就让这个客户go away
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
			if (iIndex == 0)			// 2）m_hAcceptEvent事件对象受信，说明投递的Accept请求不够，需要增加
			{
				::WSAEnumNetworkEvents(pThis->m_sListen, hWaitEvents[iIndex], &ne);
				if (ne.lNetworkEvents & FD_ACCEPT)
				{
					nLimit = 50;  // 增加的个数，这里设为50个
				}
			}
			else if (iIndex == 1)	// 3）m_hRepostEvent事件对象受信，说明处理I/O的线程接受到新的客户
			{
				nLimit = InterlockedExchange(&pThis->m_repostCount, 0);
			}
			else if (iIndex > 1)		// I/O服务线程退出，说明有错误发生，关闭服务器
			{
				pThis->m_shutDown = true;
				continue;
			}

			// 投递nLimit个AcceptEx I/O请求
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
	::OutputDebugString("	WorkerThread 启动... \n");
#endif // _DEBUG

	CIOCPServer *pThis = (CIOCPServer*)lpParam;

	CIOCPBuffer *pBuffer;
	ULONG dwKey;
	DWORD dwTrans;
	LPOVERLAPPED lpol;
	while (TRUE)
	{
		// 在关联到此完成端口的所有套节字上等待I/O完成
		BOOL bOK = ::GetQueuedCompletionStatus(pThis->m_ioCompletion,
			&dwTrans, (ULONG_PTR*)&dwKey, (LPOVERLAPPED*)&lpol, WSA_INFINITE);

		if (dwTrans == -1) // 用户通知退出
		{
#ifdef _DEBUG
			::OutputDebugString("	WorkerThread 退出 \n");
#endif // _DEBUG
			::ExitThread(0);
		}

		pBuffer = CONTAINING_RECORD(lpol, CIOCPBuffer, m_ol);
		int nError = NO_ERROR;
		if (!bOK)						// 在此套节字上有错误发生
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
	::OutputDebugString("	WorkerThread 退出 \n");
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

	// 1）首先减少套节字上的未决I/O计数
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

		// 2）检查套节字是否已经被我们关闭
		if (pContext->m_bClosed)
		{
#ifdef _DEBUG
			::OutputDebugString("	检查到套节字已经被我们关闭 \n");
#endif // _DEBUG
			if (pContext->m_iOutstandingRecvNumber == 0 && pContext->m_iOutstandingSendNumber == 0)
			{
				ReleaseContext(pContext);
			}
			// 释放已关闭套节字的未决I/O
			ReleaseBuffer(pBuffer);
			return;
		}
	}
	else
	{
		RemovePendingAccept(pBuffer);
	}

	// 3）检查套节字上发生的错误，如果有的话，通知用户，然后关闭套节字
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
			::OutputDebugString("	检查到客户套节字上发生错误 \n");
#endif // _DEBUG
		}
		else // 在监听套节字上发生错误，也就是监听套节字处理的客户出错了
		{
			// 客户端出错，释放I/O缓冲区
			if (pBuffer->m_socketClient != INVALID_SOCKET)
			{
				::closesocket(pBuffer->m_socketClient);
				pBuffer->m_socketClient = INVALID_SOCKET;
			}
#ifdef _DEBUG
			::OutputDebugString("	检查到监听套节字上发生错误 \n");
#endif // _DEBUG
		}

		ReleaseBuffer(pBuffer);
		return;
	}


	// 开始处理
	if (pBuffer->m_op == OP_ACCEPT)
	{
		if (dwTrans == 0)
		{
#ifdef _DEBUG
			::OutputDebugString("	监听套节字上客户端关闭 \n");
#endif // _DEBUG

			if (pBuffer->sClim_socketClientent != INVALID_SOCKET)
			{
				::closesocket(pBuffer->m_socketClient);
				pBuffer->m_socketClient = INVALID_SOCKET;
			}
		}
		else
		{
			// 为新接受的连接申请客户上下文对象
			CIOCPContext *pClient = AllocateContext(pBuffer->m_socketClient);
			if (pClient != NULL)
			{
				if (AddAConnection(pClient))
				{
					// 取得客户地址
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

					// 关联新连接到完成端口对象
					::CreateIoCompletionPort((HANDLE)pClient->m_socket, m_ioCompletion, (DWORD)pClient, 0);

					// 通知用户
					pBuffer->m_iLen = dwTrans;
					OnConnectionEstablished(pClient, pBuffer);

					// 向新连接投递几个Read请求，这些空间在套节字关闭或出错时释放
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
				else	// 连接数量已满，关闭连接
				{
					CloseAConnection(pClient);
					ReleaseContext(pClient);
				}
			}
			else
			{
				// 资源不足，关闭与客户的连接即可
				::closesocket(pBuffer->m_socketClient);
				pBuffer->m_socketClient = INVALID_SOCKET;
			}
		}

		// Accept请求完成，释放I/O缓冲区
		ReleaseBuffer(pBuffer);

		// 通知监听线程继续再投递一个Accept请求
		::InterlockedIncrement(&m_repostCount);
		::SetEvent(m_hRepostEvent);
	}
	else if (pBuffer->m_op == OP_READ)
	{
		if (dwTrans == 0)	// 对方关闭套节字
		{
			// 先通知用户
			pBuffer->m_iLen = 0;
			OnConnectionClosing(pContext, pBuffer);
			// 再关闭连接
			CloseAConnection(pContext);
			// 释放客户上下文和缓冲区对象
			if (pContext->m_iOutstandingRecvNumber == 0 && pContext->m_iOutstandingSendNumber == 0)
			{
				ReleaseContext(pContext);
			}
			ReleaseBuffer(pBuffer);
		}
		else
		{
			pBuffer->nLen = dwTrans;
			// 按照I/O投递的顺序读取接收到的数据
			CIOCPBuffer *p = GetNextCanReadBuffer(pContext, pBuffer);
			while (p != NULL)
			{
				// 通知用户
				OnReadCompleted(pContext, p);
				// 增加要读的序列号的值
				::InterlockedIncrement((LONG*)&pContext->m_iCurrentReadSequence);
				// 释放这个已完成的I/O
				ReleaseBuffer(p);
				p = GetNextCanReadBuffer(pContext, NULL);
			}

			// 继续投递一个新的接收请求
			pBuffer = AllocateBuffer(IOCP_BUFFER_SIZE);
			if (pBuffer == NULL || !PostRecv(pContext, pBuffer))
			{
				CloseAConnection(pContext);
			}
		}
	}
	else if (pBuffer->m_op == OP_WRITE)
	{

		if (dwTrans == 0)	// 对方关闭套节字
		{
			// 先通知用户
			pBuffer->m_iLen = 0;
			OnConnectionClosing(pContext, pBuffer);

			// 再关闭连接
			CloseAConnection(pContext);

			// 释放客户上下文和缓冲区对象
			if (pContext->m_iOutstandingRecvNumber == 0 && pContext->m_iOutstandingSendNumber == 0)
			{
				ReleaseContext(pContext);
			}
			ReleaseBuffer(pBuffer);
		}
		else
		{
			// 写操作完成，通知用户
			pBuffer->m_iLen = dwTrans;
			OnWriteCompleted(pContext, pBuffer);
			// 释放SendText函数申请的缓冲区
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
