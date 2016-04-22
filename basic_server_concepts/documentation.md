## What is a scoket?
- everything in linux is a file. Socket is a way to speak to other programs using linux file descriptors.
## 2.1 Main two types, SOCK_STREAM(stream sockets) , SOCK_DGRAM(datagram sockets)
- SOCK_STREAM, stream sockets are reliable two-way connected communication streams using TCP protocol.( If you output two items into the socket in the order "1, 2", they will arrive in the order "1, 2" at the opposite end. )
- SOCK_DGRAM, datagram sockets, connectionless. Unreliable, if you send a datagram, it may arrive, may arrive out of order, suing UDP. Why using unreliable protocol ? speed.
##2.2 low level, network thory
- (Ethernet (IP (UDP (TFTP (DATA) ) ) ) )
- 7 LAYERS: Application > Presentation > Session > Transport > Network > Data Link > Physical

## 3.0 IP addresses
### 3.1 IPv4 and IPv6, subnets and port numbers, netmask, /etc/services to find ports
### 3.2 Byte Order, Big Endian(Network Byte order), Little Endian
- htons(), host to network short
- htonl(), host to network long
- ntohs(), network to host short
- ntohl(), network to host long

### 3.3 Data types
- socket descriptor : int
- struct addrinfo

		struct addrinfo{
		int              ai_flags;     // AI_PASSIVE, AI_CANONNAME, etc.
    	int              ai_family;    // AF_INET, AF_INET6, AF_UNSPEC
    	int              ai_socktype;  // SOCK_STREAM, SOCK_DGRAM
    	int              ai_protocol;  // use 0 for "any"
    	size_t           ai_addrlen;   // size of ai_addr in bytes
    	struct sockaddr *ai_addr;      // struct sockaddr_in or _in6
    	char            *ai_canonname; // full canonical hostname
    	struct addrinfo *ai_next;      // linked list, next node`
		};

- struct sockaddr

		struct sockaddr {
    	unsigned short    sa_family;    // address family, AF_xxx
    	char              sa_data[14];  // 14 bytes of protocol address
		};

- struct sockaddr_in (ipv4 only)
 
		struct sockaddr_in {
    	short int          sin_family;  // Address family, AF_INET
    	unsigned short sin_port;    // Port number
    	struct in_addr     sin_addr;    // Internet address
		unsigned char      sin_zero[8]; // Same size as struct sockaddr
	};

- struct in_addr(ipv4 only)

		// Internet address (a structure for historical reasons)
		struct in_addr {
		uint32_t s_addr; // that's a 32-bit int (4 bytes)
		};

- struct sockaddr_in6, in6_addr
	
		struct sockaddr_in6 {
    	u_int16_t       sin6_family;   // address family, AF_INET6
    	u_int16_t       sin6_port;     // port number, Network Byte Order
    	u_int32_t       sin6_flowinfo; // IPv6 flow information
    	struct in6_addr sin6_addr;     // IPv6 address
    	u_int32_t       sin6_scope_id; // Scope ID
		};
		struct in6_addr {
    	unsigned char   s6_addr[16];   // IPv6 address
		};

- struct sockaddr_storage, both enough for ipv4 and ipv6
	
		struct sockaddr_storage {
    	sa_family_t  ss_family;     // address family
    	// all this is padding, implementation specific, ignore it:
		char      __ss_pad1[_SS_PAD1SIZE];
		int64_t   __ss_align;
		char      __ss_pad2[_SS_PAD2SIZE];
		};

### 3.4 IP addresses functions

#### 3.4.1 from ip addresses to binary representations

- ip too binary, example ipv4

		struct sockaddr_in sa;
		inet_pton(AF_INET, "192.0.2.1", &(sa.sin_addr)); // IPv4
- ip too binary, example ipv6

		struct sockaddr_in6 sa6; 
		inet_pton(AF_INET6, "2001:db8:63b3:1::3490", &(sa6.sin6_addr)); // IPv6

#### 3.4.2 from binary to ip addresses
- binary to ip addresses, ipv4

		char ip4[INET_ADDRSTRLEN]; 
		struct sockaddr_in sa; 
		inet_ntop(AF_INET, &(sa.sin_addr), ip4, INET_ADDRSTRLEN);
- binary to ip addresses,ipv6
		
		char ip6[INET6_ADDRSTRLEN];
		struct sockaddr_in6 sa6;
		inet_ntop(AF_INET6, &(sa6.sin6_addr), ip6, INET6_ADDRSTRLEN);

## 4 System calls
### 4.1 getaddrinfo(), man getaddrinfo
#### 4.1.0 prototype

	#include<sys/types.h>
	#include<sys/socket.h>
	#include<netdb.h>
	int getaddrinfo(const char *node, //"www.baidu.com" or ip address
			const char *service, //"http" or port number
			const struct addrinfo *hints,
			struct addrinfo **res);
#### 4.1.1 usage
	int status;
	struct addrinfo hints;
	struct addrinfo *serverinfo; //point to the results
	
	memset(&hints, sizeof(hints)); hints.ai_family = AF_UNSPEC;//do not care ipv4 or ipv6 
	hints.ai_socktype = SOCK_STREAM;//tcp connections
	hints.ai_flags = AI_PASSIVE;//tells getaddrinfo() to assign the address of my local host to the socket structures
	if((status == getaddrinfo(NULL, "80", &hints, &serverinfo)) != 0)
	{
		fprintf(stderr, "get addr info error : %s\n", gai_strerror(status));
		exit(1);
	}

	//server info now point to one or more struct addrinfos, you can use it
	freeaddrinfo(serverinfo);
#### 4.1.2 an example demo using getaddrinfo, demo_getaddrinfo.c
### 4.2 socket(), man socket, returns -1 on error and sets errno to the error's value.

	#include <sys/types.h>
	#include <sys/socket.h>
	int socket(int domain,//PF_INET or PF_INET6 
			int type, // SOCK_STREAM or SOCK_DGRAM
			int protocol);//tcp,udp or getprotobyname()
#### 4.2.1 PF_INET <=> AF_INET, have the same value, PROTOCOL FAMILIES, ADDRESS families
	
	getaddrinfo(host, service, &hints, &result);
	socketfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	
### 4.3 bind(),man bind, returns -1 on error and sets errno to the error's value.
#### 4.3.1 the port number is used by the kernel to match an incoming packet to a certain process's socket descriptor.

	#include <sys/types.h>
	#include <sys/socket.h>
	int bind(int sockfd, struct sockaddr *my_addr, int addrlen);

	//new ways of bind
	getaddrinfo(NULL, port, &hints, &result);
	socketfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	bind(sockfd, result->ai_addr, result->ai_addrlen)

	//old ways of bind,ipv4
	struct sockaddr_in addr;
	sockfd = socket(PF_INET, SOCK_STREAM, 0);
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(80);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	bind(sockfd, (struct addrinfo*) addr, sizeof(addr));

#### 4.3.2 INADDR_ANY, in6addr_any( bind to your local IP address)
#### 4.3.3 Do not use port number under 1024
#### 4.3.4 set reuse the port

	int yes = 1;
	if(setsockopt(listener, SOL_SOCKET, SO_REUSERADDR, &yes, sizeof(yes)) == -1)
	{
		perror("setsockopt erorr");
		exit(1);
	}

#### 4.3.5 no need to call bind() when the program is client-side, use connect(), connect() will bind() it to un unused local port if necessary.
### 4.4 connect(), return -1 on error and set the variable errno.

	#include <sys/types.h>
	#include <sys/socket.h>
	int connect(int sockfd, struct sockaddr *serv_addr, int addrlen); 
### 4.5 listen(), returns -1 and sets errno on error.
	
	int listen(int sockfd, int backlog);//backlog is the number of connections allowed on the incoming queue.
	//getaddrinfo() -> socket() -> bind() -> listen() -> accept()

### 4.6 accept(),returns -1 and sets errno if an error occurs.
clients call connect() to your machine that you are listen() on. Their connections will be queued up to be accept()ed. You call accept() and tell it to get the pending connection. It'll return you a new socket file descriptor to use for this single connection. The original socker file descriptor is listening for more new connections, and the newly created one id finally ready to send() and recv().

	#include <sys/types.h>
	#include <sys/socket.h>
	
	int accept(int sockfd, //the listening socket 
	struct sockaddr *addr,//the incoming info, host and port 
	socklen_t *addrlen);

### 4.7 send() and recv(),man recv,man send

	int send(int sockfd, //the socketfd you want to send data to 
	const void *msg, int len, int flags); 
	//send() returns the number of bytes actually sent out-maybe less than len.

	int recv(int sockfd, void *buf, int len, int flags);
	//recv() returns the number of bytes actually read into the buffer, or -1 errno set
### 4.8 close() and shutdown(), man 2 shutdown

	close(sockfd);	
	shutdown(int sockfd, int how);
## 5 Serve rand Client examples
### 5.1 server example

### 5.2 client example

First of all, try to use getaddrinfo() to get all the struct sockaddr info, instead of packing the structures by hand. This will keep you IP version-agnostic, and will eliminate many of the subsequent steps.
Any place that you find you're hard-coding anything related to the IP version, try to wrap up in a helper function.
Change AF_INET to AF_INET6.
Change PF_INET to PF_INET6.
Change INADDR_ANY assignments to in6addr_any assignments, which are slightly different:

struct sockaddr_in sa;
struct sockaddr_in6 sa6;

sa.sin_addr.s_addr = INADDR_ANY;  // use my IPv4 address
sa6.sin6_addr = in6addr_any; // use my IPv6 address
Also, the value IN6ADDR_ANY_INIT can be used as an initializer when the struct in6_addr is declared, like so:

struct in6_addr ia6 = IN6ADDR_ANY_INIT;
Instead of struct sockaddr_in use struct sockaddr_in6, being sure to add "6" to the fields as appropriate (see structs, above). There is no sin6_zero field.
Instead of struct in_addr use struct in6_addr, being sure to add "6" to the fields as appropriate (see structs, above).
Instead of inet_aton() or inet_addr(), use inet_pton().
Instead of inet_ntoa(), use inet_ntop().
Instead of gethostbyname(), use the superior getaddrinfo().
Instead of gethostbyaddr(), use the superior getnameinfo() (although gethostbyaddr() can still work with IPv6).
INADDR_BROADCAST no longer works. Use IPv6 multicast instead.