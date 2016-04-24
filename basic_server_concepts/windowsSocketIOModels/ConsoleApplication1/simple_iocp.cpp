#include "CinitSock.h"

DWORD WINAPI SimpleIOCPThread(LPVOID lpParam)
{
	// 得到完成端口句柄
	HANDLE hCompletion = (HANDLE)lpParam;
	DWORD dwTrans;
	PPER_HANDLE_DATA pHandle;
	PPER_IO_DATA pIOData;
	while (true)
	{
		// 在关联到此完成端口的所有套接字上，等待IO完成
		bool bOk = ::GetQueuedCompletionStatus(hCompletion,
			&dwTrans, (PULONG_PTR)&pHandle, (LPOVERLAPPED*)&pIOData, WSA_INFINITE);

		if (!bOk)
		{
			::closesocket(pHandle->m_socket);
			::GlobalFree(pHandle);
			::GlobalFree(pIOData);
			continue;
		}

		if (dwTrans == 0 && (pIOData->m_op == OP_READ || pIOData->m_op == OP_WRITE))
		{
			::closesocket(pHandle->m_socket);
			::GlobalFree(pHandle);
			::GlobalFree(pIOData);
			continue;
		}

		switch (pIOData->m_op)
		{
		case OP_READ:
			{
				pIOData->m_buff[dwTrans] = '\0';
				printf("recv data:%s", pIOData->m_buff);
				// 继续投递IO请求
				WSABUF buf;
				buf.buf = pIOData->m_buff;
				buf.len = maxBufferLen;
				pIOData->m_op = OP_READ;
				DWORD iFlags = 0;
				::WSARecv(pHandle->m_socket, &buf, 1, &dwTrans, &iFlags, &pIOData->m_overlapped, 0);
			}
			break;
		case OP_WRITE:
		case OP_ACCEPT:
			{}
			break;
		default:
			break;
		}
	}
	return 0;
}

int do_simple_iocp()
{
	HANDLE hCompletion = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	::CreateThread(NULL, 0, SimpleIOCPThread, (LPVOID)hCompletion, 0, 0);

	SOCKET sListen = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(bindPort);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;
	::bind(sListen, (const sockaddr*)&sin, sizeof(sin));
	::listen(sListen, 20);

	while (true)
	{
		sockaddr_in saRemote;
		int nRemoteLen = sizeof(saRemote);
		SOCKET sNew = ::accept(sListen, (sockaddr*)&saRemote, &nRemoteLen);
		//接收到新连接之后,为它创建一个 per-handle 数据,并关联到莞城端口对象
		PPER_HANDLE_DATA pHandle = (PPER_HANDLE_DATA)::GlobalAlloc(GPTR, sizeof(PPER_HANDLE_DATA));
		if (!pHandle)
		{
			printf("can not allocate memory,stop");
			return -1;
		}
		pHandle->m_socket = sNew;
		memcpy(&pHandle->m_clientAddr, &saRemote, sizeof(pHandle->m_clientAddr));
		::CreateIoCompletionPort((HANDLE)pHandle->m_socket, hCompletion, (ULONG_PTR)pHandle, 0);

		// 投递一个接收请求
		PPER_IO_DATA pIOData = (PPER_IO_DATA)::GlobalAlloc(GPTR, sizeof(PER_IO_DATA));
		pIOData->m_op = OP_READ;
		WSABUF buf;
		buf.buf = pIOData->m_buff;
		buf.len = maxBufferLen;
		DWORD dwRecv;
		DWORD dwFlags = 0;
		::WSARecv(pHandle->m_socket, &buf, 1, &dwRecv, &dwFlags, &pIOData->m_overlapped, NULL);
	}
	return 0;
}