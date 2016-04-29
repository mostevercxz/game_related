#include "ciocp.h"

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

CIOCPBuffer * CIOCPServer::GetNextCanReadBuffer(CIOCPContext *pContext, CIOCPBuffer *pBUffer)
{

}