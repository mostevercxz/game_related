#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>//memset, strerror
#include <stdio.h>//snprintf
#include <iostream>
#include <arpa/inet.h>
#include <errno.h>

// Usage: ./demo_socket_server

class BaseSocketServer
{
	private:
					int m_sockfd;
					unsigned short m_listenport;//the port the server is listening

	public:
					BaseSocketServer(const unsigned short port=12000);
					void Run();//while loop, keep accepting
					bool InitSocket();//create a socket, and bind to port , and listen
					void * get_in_addr(struct sockaddr*);
};

int main()
{
	BaseSocketServer server;
	server.InitSocket();
	server.Run();
	return 0;
}

BaseSocketServer::BaseSocketServer(const unsigned short port):m_listenport(port) { }

// get sockaddr, IPv4 or IPv6:
void *BaseSocketServer::get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void BaseSocketServer::Run()
{
	std::cout << "Now, waiting for the connection on port " << m_listenport << std::endl;
	struct sockaddr_storage their_addr;
	while(true)
	{
		socklen_t size = sizeof(their_addr);
		int new_fd = accept(m_sockfd, (struct sockaddr*)&their_addr, &size);
		if(-1 == new_fd)
		{
			std::cerr << "accept error, error code" << strerror(errno) << std::endl;
			break;
		}//end if

		char ipstr[INET6_ADDRSTRLEN];
		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr*)&their_addr), ipstr, sizeof(ipstr));
		std::cout << "receive connection from " << ipstr	<< std::endl;
		int status = send(new_fd, "Hello", 5, 0);
		if(status == -1)
		{
			std::cerr << "send error, error code" << strerror(errno) << std::endl;
			break;
		}
	}//end while
}

bool BaseSocketServer::InitSocket()
{
	//1. Use getaddrinfo, to get a struct addrinfo
	struct addrinfo hints, *result;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;//fill my local ip for me

	char portstr[8];
	snprintf(portstr, sizeof(portstr), "%hu", m_listenport);

	int status = getaddrinfo(NULL, portstr, &hints, &result);
	if(status != 0)
	{
		std::cerr << "get addrinfo failed" << gai_strerror(status) << std::endl;
		return false;
	}

	if(result)
	{
		m_sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if(-1 == m_sockfd)
		{
			std::cerr << "create socket connection error, error code" << strerror(errno) << std::endl;
			return false;
		}
	}//end if
	else
	{
		return false;
	}

	status = bind(m_sockfd, result->ai_addr, result->ai_addrlen);
	if(-1 == status)
	{
		std::cerr << "bind socket connection error, error code" << strerror(errno) << std::endl;
		return false;
	}

	freeaddrinfo(result);

	status = listen(m_sockfd, 1);//listen to one connection
	if(-1 == status)
	{
		std::cerr << "listen socket connection error, error code" << strerror(errno) << std::endl;
		return false;
	}

	return true;
}
