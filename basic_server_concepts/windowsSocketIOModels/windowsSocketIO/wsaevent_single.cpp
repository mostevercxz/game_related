#include "CinitSock.h"

int do_wsaevent_single_thread()
{
	WSAEVENT eventArray[WSA_MAXIMUM_WAIT_EVENTS];
	SOCKET sockArray[WSA_MAXIMUM_WAIT_EVENTS];
	int iEventTotal = 0;

	SOCKET sListen = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(bindPort);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;
	if (::bind(sListen, (const sockaddr*)&sin, sizeof(sin)) == SOCKET_ERROR)
	{
		printf("failed to bind,error:");
		return 1;
	}

	::listen(sListen, 5);
	//�����¼�����,���������׽���
	WSAEVENT event = ::WSACreateEvent();
	::WSAEventSelect(sListen, event, FD_ACCEPT | FD_CLOSE);
	// ��ӵ�ȫ��������
	eventArray[iEventTotal] = event;
	sockArray[iEventTotal] = sListen;
	++iEventTotal;

	//���������¼�
	while (true)
	{
		int iIndex = ::WSAWaitForMultipleEvents(iEventTotal, eventArray, false, WSA_INFINITE, false);
		// ��ÿ���¼����� WSAWaitForMultipleEvents ����,��ȷ������״̬

	}
}