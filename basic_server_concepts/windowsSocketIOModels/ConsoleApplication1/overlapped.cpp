#include "CinitSock.h"

// Ϊ�˵��� WSAWaitForMultipleEvents �������ύ��IO�ϵȴ�,Ҫ������ m_ol.hEvent ���һ��ʱ������
HANDLE g_events[WSA_MAXIMUM_WAIT_EVENTS];
int g_iBufferCount;
POVERLAPPED_BUFFER_OBJ g_bufferHead, g_BufferTail;

POVERLAPPED_SOCKET_OBJ NewOverlappedSocketObj(SOCKET s)
{
	POVERLAPPED_SOCKET_OBJ pSocket = (POVERLAPPED_SOCKET_OBJ)::GlobalAlloc(GPTR, sizeof(OVERLAPPED_SOCKET_OBJ));
	if (pSocket)
	{
		pSocket->m_socket = s;
	}
	return pSocket;
}

void FreeOverlappedSocketObj(POVERLAPPED_SOCKET_OBJ pSocket)
{
	if (pSocket->m_socket != INVALID_SOCKET)
	{
		::closesocket(pSocket->m_socket);
	}

	::GlobalFree(pSocket);
}

POVERLAPPED_BUFFER_OBJ NewBufferObj(POVERLAPPED_SOCKET_OBJ pSocket, ULONG nLen)
{
	if (g_iBufferCount > WSA_MAXIMUM_WAIT_EVENTS - 1)
	{
		return NULL;
	}

	POVERLAPPED_BUFFER_OBJ pBuffer = (POVERLAPPED_BUFFER_OBJ)::GlobalAlloc(GPTR, sizeof(OVERLAPPED_BUFFER_OBJ));
	if (pBuffer)
	{
		pBuffer->m_buff = (char*)::GlobalAlloc(GPTR, nLen);
		pBuffer->m_iLen = nLen;
		pBuffer->m_pSocket = pSocket;
		pBuffer->m_ol.hEvent = ::WSACreateEvent();
		pBuffer->m_accpetSocket = INVALID_SOCKET;

		// ���µ� BUFFER_OBJ ��ӵ�����
		if (!g_bufferHead)
		{
			g_bufferHead = g_BufferTail = pBuffer;
		}
		else
		{
			g_BufferTail->m_pNext = pBuffer;
			g_BufferTail = pBuffer;
		}
		g_events[g_iBufferCount++] = pBuffer->m_ol.hEvent;
	}

	return pBuffer;
}

// �ͷ� buffer object
void FreeOverlappedBufferObj(POVERLAPPED_BUFFER_OBJ pBuffer)
{
	// ��ȫ���б����Ƴ� buffer ����
	POVERLAPPED_BUFFER_OBJ pTemp = g_bufferHead;
	bool bFind = false;
	if (pTemp == pBuffer)
	{
		g_bufferHead = g_BufferTail = NULL;
		bFind = true;
	}
	else
	{
		while (pTemp && pTemp->m_pNext != pBuffer)
		{
			pTemp = pTemp->m_pNext;
		}

		if (pTemp)
		{
			pTemp->m_pNext = pBuffer->m_pNext;
			if (pBuffer == g_BufferTail)
			{
				g_BufferTail = pTemp;				
			}			
			bFind = true;
		}		
	}

	// �ͷ� oerlappedObject ռ�õ��ڴ�ռ�
	if (bFind)
	{
		--g_iBufferCount;
		::CloseHandle(pBuffer->m_ol.hEvent);
		::GlobalFree(pBuffer->m_buff);
		::GlobalFree(pBuffer);
	}
}

// �ص�IO��ɺ󣬵õ��������¼�����ľ��
POVERLAPPED_BUFFER_OBJ FindBufferByHandle(HANDLE hEvent)
{
	POVERLAPPED_BUFFER_OBJ pBuffer = g_bufferHead;
	while (pBuffer)
	{
		if (pBuffer->m_ol.hEvent == hEvent)
		{
			break;
		}
		pBuffer = pBuffer->m_pNext;
	}

	return pBuffer;
}

// �ؽ� g_events ����
void RebuildArray()
{
	POVERLAPPED_BUFFER_OBJ pBuffer = g_bufferHead;
	int i = 1;
	while (pBuffer)
	{
		g_events[i++] = pBuffer->m_ol.hEvent;
		pBuffer = pBuffer->m_pNext;
	}
}

// 1. ����IO����,�����׽����ϵ��ص�IO����
// 2.Ͷ���ص�IO
bool PostAccept(POVERLAPPED_BUFFER_OBJ pBuffer)
{
	POVERLAPPED_SOCKET_OBJ pSocket = pBuffer->m_pSocket;
	if (pSocket->m_acceptEx)
	{
		// ����IO����,�����׽����ϵ��ص�IO����
		pBuffer->m_op = OP_ACCEPT;
		++(pSocket->m_iOverlappingNumber);
		// Ͷ�ݴ��ص�IO
		DWORD dwBytes;
		pBuffer->m_accpetSocket = ::WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		bool b = pSocket->m_acceptEx(pSocket->m_socket,
			pBuffer->m_accpetSocket,
			pBuffer->m_buff,
			maxBufferLen - ((sizeof(sockaddr_in) + 16) * 2),
			sizeof(sockaddr_in) + 16,
			sizeof(sockaddr_in) + 16,
			&dwBytes,
			&pBuffer->m_ol
		);

		if (!b)
		{
			int errorCode = ::WSAGetLastError();
			if (errorCode != WSA_IO_PENDING)
			{
				printf("sockid=%d,PostAccept error,code=%d", pBuffer->m_pSocket->m_socket, errorCode);
				return false;
			}
		}

		return true;
	}
	return false;
}

bool PostRecv(POVERLAPPED_BUFFER_OBJ pBuffer)
{
	pBuffer->m_op = OP_READ;
	pBuffer->m_pSocket->m_iOverlappingNumber++;

	DWORD dwBytes;
	DWORD dwFlags = 0;
	WSABUF buf;
	buf.buf = pBuffer->m_buff;
	buf.len = pBuffer->m_iLen;
	if (::WSARecv(pBuffer->m_pSocket->m_socket, &buf, 1, &dwBytes, &dwFlags, &pBuffer->m_ol, NULL) != NO_ERROR)
	{
		int errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			printf("sockid=%d,PostRecv error,code=%d", pBuffer->m_pSocket->m_socket, errorCode);
			return false;
		}
	}
	return true;
}

bool PostSend(POVERLAPPED_BUFFER_OBJ pBuffer)
{
	pBuffer->m_op = OP_WRITE;
	pBuffer->m_pSocket->m_iOverlappingNumber++;

	DWORD dwBytes;
	DWORD dwFlags = 0;
	WSABUF buf;
	buf.buf = pBuffer->m_buff;
	buf.len = pBuffer->m_iLen;
	if (::WSASend(pBuffer->m_pSocket->m_socket, &buf, 1, &dwBytes, dwFlags, &pBuffer->m_ol, NULL) != NO_ERROR)
	{
		int errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			printf("sockid=%d,PostSend error,code=%d", pBuffer->m_pSocket->m_socket, errorCode);
			return false;
		}
	}
	return true;
}

bool handleOverlappedIO(POVERLAPPED_BUFFER_OBJ pBuffer)
{
	POVERLAPPED_SOCKET_OBJ pSocket = pBuffer->m_pSocket;
	pSocket->m_iOverlappingNumber--;

	// ��ȡ�ص�IO�������
	DWORD dwTrans;
	DWORD dwFalgs;
	bool bRet = ::WSAGetOverlappedResult(pSocket->m_socket, &pBuffer->m_ol, &dwTrans, false, &dwFalgs);
	if (!bRet)
	{
		// ���׽������д�����,�ر��׽���,�Ƴ�����������
		if (pSocket->m_socket != INVALID_SOCKET)
		{
			::closesocket(pSocket->m_socket);
			pSocket->m_socket = INVALID_SOCKET;
		}

		if (0 == pSocket->m_iOverlappingNumber)
		{
			FreeOverlappedSocketObj(pSocket);
		}

		FreeOverlappedBufferObj(pBuffer);
		return false;
	}

	switch (pBuffer->m_op)
	{
	case OP_ACCEPT: //���յ�������,���յ��Է������ĵ�һ�����
		{
			// Ϊ�����Ӵ���һ�� OVERLAPPED_SOCKET_OBJ ����
			POVERLAPPED_SOCKET_OBJ pClient = NewOverlappedSocketObj(pBuffer->m_accpetSocket);
			POVERLAPPED_BUFFER_OBJ pSendBuffer = NewBufferObj(pClient, maxBufferLen);
			if (!pSendBuffer)
			{
				printf("too many connections!! count=%d", g_iBufferCount);
				FreeOverlappedSocketObj(pClient);
				return false;
			}

			RebuildArray();

			// �����ݸ��Ƶ����ͻ�����
			pSendBuffer->m_iLen = dwTrans;
			memcpy(pSendBuffer->m_buff, pBuffer->m_buff, dwTrans);
			if (!PostSend(pSendBuffer))
			{
				printf("can not PostSend()");
				FreeOverlappedSocketObj(pClient);
				FreeOverlappedBufferObj(pSendBuffer);
				return false;
			}

			PostAccept(pBuffer);
		}
		break;
	case OP_READ:
		{
			// �����������
			if (dwTrans > 0)
			{
				POVERLAPPED_BUFFER_OBJ pSend = pBuffer;
				pSend->m_iLen = dwTrans;
				PostSend(pSend);// �����ݻ��Ը��ͻ���
			}
			else
			{
				// �׽��ֹر�
				if (pSocket->m_socket != INVALID_SOCKET)
				{
					::closesocket(pSocket->m_socket);
					pSocket->m_socket = INVALID_SOCKET;
				}

				if (0 == pSocket->m_iOverlappingNumber)
				{
					FreeOverlappedSocketObj(pSocket);
				}

				FreeOverlappedBufferObj(pBuffer);
				return false;
			}
		}
		break;
	case OP_WRITE:
		{
			// �����������,����ʹ�øû�������Ͷ�ݽ������ݵ�����
			if (dwTrans > 0)
			{
				pBuffer->m_iLen = maxBufferLen;
				PostRecv(pBuffer);
			}
			else
			{
				// �׽��ֹر�,���Ż���һ������..
				if (pSocket->m_socket != INVALID_SOCKET)
				{
					::closesocket(pSocket->m_socket);
					pSocket->m_socket = INVALID_SOCKET;
				}

				if (0 == pSocket->m_iOverlappingNumber)
				{
					FreeOverlappedSocketObj(pSocket);
				}

				FreeOverlappedBufferObj(pBuffer);
			}
		}
		break;
	default:
		break;
	}

	return true;
}

int do_overlapped()
{
	SOCKET sListen = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(bindPort);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;
	::bind(sListen, (const sockaddr*)&sin, sizeof(sin));
	::listen(sListen, 20);

	// Ϊ������ socket ����һ�� OVERLAPPED_SOCKET_OBJ ����
	POVERLAPPED_SOCKET_OBJ pListen = NewOverlappedSocketObj(sListen);
	if (!pListen)
	{
		printf("socket create error!");
		return -1;
	}

	// ������չ���� AcceptEx
	GUID guidAcceptEx = WSAID_ACCEPTEX;
	DWORD dwBytes;
	WSAIoctl(pListen->m_socket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guidAcceptEx, sizeof(guidAcceptEx),
		&pListen->m_acceptEx,
		sizeof(pListen->m_acceptEx),
		&dwBytes,
		NULL,
		NULL
	);

	// �����������½��� g_events ������¼�
	g_events[0] = ::WSACreateEvent();
	++g_iBufferCount;
	for (int i = 0; i < 5; ++i)
	{
		PostAccept(NewBufferObj(pListen, maxBufferLen));
	}

	// main loop
	while (true)
	{
		int iIndex = ::WSAWaitForMultipleEvents(g_iBufferCount, g_events, false, WSA_INFINITE, false);
		if (iIndex == WSA_WAIT_FAILED)
		{
			int errorCode = WSAGetLastError();
			printf("WSAWaitForMultipleEvents() failed,stop,errocode=%d\n", errorCode);
			break;
		}

		iIndex -= WSA_WAIT_EVENT_0;
		for (int i = iIndex; i < g_iBufferCount + 1; ++i)
		{
			// ���β鿴ÿ��event �Ƿ�����
			int iEveryEvent = ::WSAWaitForMultipleEvents(1, &g_events[i], true, 0, false);
			if (iEveryEvent  == WSA_WAIT_TIMEOUT)
			{
				continue;
			}
			else
			{
				::WSAResetEvent(g_events[i]);
				if (0 == i)
				{
					RebuildArray();
					continue;
				}

				POVERLAPPED_BUFFER_OBJ pBuffer = FindBufferByHandle(g_events[i]);
				if (pBuffer)
				{
					if (!handleOverlappedIO(pBuffer))
					{
						printf("handle event=%d failed, remove the socket\n", g_events[i]);
						RebuildArray();
					}
				}
			}
		}
	}

	return 0;
}