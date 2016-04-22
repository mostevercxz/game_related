// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "CinitSock.h"


CInitSock theSock;

int main()
{
	//return do_select();
	return do_wsaasync();
}

