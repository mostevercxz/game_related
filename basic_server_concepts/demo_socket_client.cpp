#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

class BaseSocketClient
{
	private:
					std::string m_destIp;
					unsigned short m_destPort;
					int m_sockfd;

	public:
					BaseSocketClient(const std::string &ip="127.0.0.1", const unsigned short port = 12000);
					bool InitSocket();
					void * get_in_addr(struct sockaddr*);
					void Run();//receive commands from the client
};

int main()
{
	BaseSocketClient client;
	if(client.InitSocket())
	{
		client.Run();
	}
	return 0;
}

BaseSocketClient::BaseSocketClient(const std::string &ip, const unsigned short port)
{
	m_destIp = ip;
	m_destPort = port;
}
// get sockaddr, IPv4 or IPv6:
void *BaseSocketClient::get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void BaseSocketClient::Run()
{
	char buf[100]={0};
	while(true)
	{
		int numberbytes = recv(m_sockfd, buf, sizeof(buf)-1, 0);
		if(numberbytes == -1)
		{
			std::cerr << "error occurs when recv(), code " << strerror(errno) << std::endl;
			break;
		}
	}
}

bool BaseSocketClient::InitSocket()
{
	struct addrinfo hints, *result;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_UNSPEC;

	char portstr[8]={0};
	snprintf(portstr, sizeof(portstr), "%d", m_destPort);
	int retcode = getaddrinfo(m_destIp.c_str(), portstr, &hints, &result);
	if(0 != retcode || !result)
	{
		std::cerr << "getaddrinfo failed ,error code " << gai_strerror(retcode) << std::endl;
		return false;
	}

	m_sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if(-1 == m_sockfd)
	{
		std::cerr << "socket failed ,error code " << strerror(errno) << std::endl;
		return false;
	}

	retcode = connect(m_sockfd, result->ai_addr, result->ai_addrlen);
	if(0 != retcode)
	{
		std::cerr << "connect failed ,error code " << strerror(errno) << std::endl;
		return false;
	}

	char ipstr[INET6_ADDRSTRLEN]={0};
	inet_ntop(result->ai_family, get_in_addr((struct sockaddr*)result->ai_addr), ipstr, sizeof(ipstr));
	std::cout << "connect to " << ipstr << std::endl;

	freeaddrinfo(result);

	return true;

}
