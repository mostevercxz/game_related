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
	// FD_CLR(s, *set),从set中移除套接字s
	// FD_ISSET(s, *set),检查s是否为set的成员
	// FD_SET(s, *set),添加套接字到集合
	fd_set fdSets;
	FD_ZERO(&fdSets);
	FD_SET(sListen, &fdSets);
	std::vector<SOCKET> toBeCleared;
	while (true)
	{
		//copy fd_set to temp fd_set and use select
		fd_set fdRead = fdSets;
		// 当 select 返回时，它通过移除没有未决I/O 操作的套接字句柄修改每个fd_set 集合。
		// 例如，想要测试套接字s 是否可读时，必须将它添加到readfds 集合，然后等待select 函数返回。当select 调用完成后再确定s 是否仍然还在readfds 集合中，如果还在，就说明s 可读了
		int nRet = ::select(0, &fdRead, NULL, NULL, NULL);
		if (nRet > 0)
		{
			for (DWORD i = 0; i < fdSets.fd_count; ++i)
			{
				if (FD_ISSET(fdSets.fd_array[i], &fdRead))//select后还留在fdRead里,说明这个socketfd有读事件
				{
					if (fdSets.fd_array[i] == sListen)//调accept
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
						//说明客户端发送数据过来了,接受下
						char buffer[maxBufferLen] = { 0 };
						int nRecv = ::recv(fdSets.fd_array[i], buffer, maxBufferLen, 0);
						if (nRecv > 0)//can read data
						{
							buffer[nRecv] = '\0';
							printf("recv data :%s\n", buffer);
						}
						else//连接关闭,重启或者中断了
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