#include "CinitSock.h"

int do_select()
{
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
	//step 1 : initialize a fd_set
	// FD_ZERO(*set)
	// FD_CLR(s, *set),��set���Ƴ��׽���s
	// FD_ISSET(s, *set),���s�Ƿ�Ϊset�ĳ�Ա
	// FD_SET(s, *set),����׽��ֵ�����
	fd_set fdSets;
	FD_ZERO(&fdSets);
	FD_SET(sListen, &fdSets);
	std::vector<SOCKET> toBeCleared;
	while (true)
	{
		//copy fd_set to temp fd_set and use select
		fd_set fdRead = fdSets;
		// �� select ����ʱ����ͨ���Ƴ�û��δ��I/O �������׽��־���޸�ÿ��fd_set ���ϡ�
		// ���磬��Ҫ�����׽���s �Ƿ�ɶ�ʱ�����뽫����ӵ�readfds ���ϣ�Ȼ��ȴ�select �������ء���select ������ɺ���ȷ��s �Ƿ���Ȼ����readfds �����У�������ڣ���˵��s �ɶ���
		int nRet = ::select(0, &fdRead, NULL, NULL, NULL);
		if (nRet > 0)
		{
			for (DWORD i = 0; i < fdSets.fd_count; ++i)
			{
				if (FD_ISSET(fdSets.fd_array[i], &fdRead))//select������fdRead��,˵�����socketfd�ж��¼�
				{
					if (fdSets.fd_array[i] == sListen)//��accept
					{
						if (fdSets.fd_count < FD_SETSIZE)
						{
							sockaddr_in remoteAddr;
							int nLen = sizeof(remoteAddr);
							SOCKET sNew = ::accept(sListen, (sockaddr*)&remoteAddr, &nLen);
							FD_SET(sNew, &fdSets);
							char ip[16] = { 0 };
							inet_ntop(AF_INET, (void*)&remoteAddr.sin_addr, ip, sizeof(ip));
							printf("successfully accept connection from %s", ip);
						}
						else
						{
							printf("too many connections");
							continue;
						}
					}
					else
					{
						//˵���ͻ��˷������ݹ�����,������
						char buffer[maxBufferLen] = { 0 };
						int nRecv = ::recv(fdSets.fd_array[i], buffer, maxBufferLen, 0);
						if (nRecv > 0)//can read data
						{
							buffer[nRecv] = '\0';
							printf("recv data :%s\n", buffer);
						}
						else//���ӹر�,���������ж���
						{
							::closesocket(fdSets.fd_array[i]);
							printf("recv data error\n");
							toBeCleared.push_back(fdSets.fd_array[i]);
						}
					}
				}
			}
			for (auto &i : toBeCleared)
			{
				FD_CLR(i, &fdSets);
			}
		}
		else if (nRet == 0)
		{
			printf("time limit expired");
		}
		else
		{
			printf("select error, server stoped");
			break;
		}
	}
}