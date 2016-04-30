// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "CinitSock.h"
#include "ciocp.h"


CInitSock theSock;

int main()
{
	//return do_select();
	//return do_wsaasync();
	//return do_wsaevent_threadpool();
	//return do_overlapped();
	//return do_simple_iocp();
	CMyServer *pServer = new CMyServer;

	// ��������
	if (pServer->Start())
	{
		printf(" �����������ɹ�... \n");
	}
	else
	{
		printf(" ����������ʧ�ܣ�\n");
		return 0;
	}

	// �����¼�������ServerShutdown�����ܹ��ر��Լ�
	HANDLE hEvent = ::CreateEvent(NULL, FALSE, FALSE, L"ShutdownEvent");
	::WaitForSingleObject(hEvent, INFINITE);
	::CloseHandle(hEvent);

	// �رշ���
	pServer->Shutdown();
	delete pServer;

	printf(" �������ر� \n ");
	return 0;
}

