#include "ciocp.h"

CIOCPBuffer * CIOCPServer::AllocateBuffer(int iLen)
{
	CIOCPBuffer *pBuffer = NULL;
	if (iLen > IOCP_BUFFER_SIZE)
	{
		return NULL;
	}

	::EnterCriticalSection(&m_freeBufferListLock);
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
	::LeaveCriticalSection(&m_freeBufferListLock);

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
	::EnterCriticalSection(&m_freeContextListLock);
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
	::LeaveCriticalSection(&m_freeContextListLock);

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

	::EnterCriticalSection(&m_freeContextListLock);
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
	::LeaveCriticalSection(&m_freeContextListLock);
}

void CIOCPServer::FreeAllBuffers()
{
	// 遍历m_pFreeBufferList空闲列表，释放缓冲区池内存
	::EnterCriticalSection(&m_freeBufferListLock);

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

	::LeaveCriticalSection(&m_freeBufferListLock);
}

void CIOCPServer::FreeAllContexts()
{
	// 遍历m_pFreeContextList空闲列表，释放缓冲区池内存
	::EnterCriticalSection(&m_freeContextListLock);

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

	::LeaveCriticalSection(&m_freeContextListLock);
}
