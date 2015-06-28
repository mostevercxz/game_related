#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

/*
 * Usage : ./demo_getaddrindo www.taobao.com
 * print the ip addresses for the host on the command line
 */

int main(int argc, char **argv)
{
	// 0.check argument
	if(2 != argc)
	{
		fprintf(stderr, "Usage : ./demo_getaddrindo hostname\n");
		return 1;
	}
	// 1. use getaddrinfo to get a addrinfo list
	struct addrinfo hints, *result;
	int status = 0;

	memset(&hints,0, sizeof(hints));//this is necessary!! or you'll get error getaddrinfo:ai_socktype not supported
	hints.ai_family = AF_UNSPEC;//both ipv4 and ipv6
	hints.ai_socktype = SOCK_STREAM;//TCP connection
	if((status = getaddrinfo(argv[1], NULL, &hints, &result)) != 0)
	{
		fprintf(stderr, "error,getaddrinfo:%s\n", gai_strerror(status));
		return 2;
	}
	// 2. analyze th list, and get each info
	struct addrinfo *p;
	for(p = result; p!= NULL; p = p->ai_next)
	{
		if(p->ai_family == AF_INET)//ipv4
		{
			struct sockaddr_in *ipv4 = (struct sockaddr_in*)p->ai_addr;
			char temp[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &(ipv4->sin_addr), temp, sizeof(temp));
			printf("IPV4 address : %s\n", temp);
		}
		else if(p->ai_family == AF_INET6)
		{
			struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)p->ai_addr;
			char temp[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, &(ipv6->sin6_addr), temp, sizeof(temp));
			printf("IPV6 address : %s\n", temp);
		}
	}

	freeaddrinfo(result);

	return 0;
}
