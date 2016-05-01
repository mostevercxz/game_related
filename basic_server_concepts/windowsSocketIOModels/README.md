What is the difference between CRITICAL_SECTION and mutex ??

some funcs:

[using mutex objects](https://msdn.microsoft.com/en-us/library/windows/desktop/ms686927.aspx)

[WaitForSingleObject()](https://msdn.microsoft.com/en-us/library/windows/desktop/ms687032.aspx)

[WSASend() function](https://msdn.microsoft.com/en-us/library/ms742203.aspx)

[GetQueuedCompletionStatus () function](https://msdn.microsoft.com/en-us/library/aa364986.aspx)

[CONTAINING_RECORD macro](https://msdn.microsoft.com/en-us/library/ff542043.aspx)

[simple iocp server](http://www.codeproject.com/Articles/13382/A-simple-application-using-I-O-Completion-Ports-an)

[reusable,high performance socket server class, part 1](http://www.codeproject.com/Articles/2336/A-reusable-high-performance-socket-server-class-Pa)

[reusable,high performance socket server class, part 2](http://www.codeproject.com/Articles/2337/A-reusable-high-performance-socket-server-class)

[reusable,high performance socket server class, part 3](http://www.codeproject.com/Articles/2374/A-reusable-high-performance-socket-server-class)

[Handling multiple pending socket read and write operations](http://www.codeproject.com/Articles/2610/Handling-multiple-pending-socket-read-and-write-op)

---
IOCP functions:

    HANDLE WINAPI CreateIoCompletionPort(
      _In_ HANDLEFileHandle,
      _In_opt_ HANDLEExistingCompletionPort,
      _In_ ULONG_PTR CompletionKey,
      _In_ DWORD NumberOfConcurrentThreads
    );
	// 创建完成端口
    ioCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0 );

	// 关联新连接到完成端口对象， 传参数 pClient 比较关键,在检查 io port 完成状态的时候会返回 pClient.
	// It is returned in the GetQueuedCompletionStatus function call when a completion packet arrives. The CompletionKey parameter is also used by the PostQueuedCompletionStatus function to queue your own special-purpose completion packets.
	CreateIoCompletionPort((HANDLE)pClient->m_socket, ioCompletionPort, (DWORD)pClient, 0);

    BOOL WINAPI GetQueuedCompletionStatus(
      _In_  HANDLE   CompletionPort,
      _Out_ LPDWORD  lpNumberOfBytes,
      _Out_ PULONG_PTR   lpCompletionKey,
      _Out_ LPOVERLAPPED *lpOverlapped,
      _In_  DWORDdwMilliseconds
    );
	// 在关联到此完成端口的所有套节字上等待I/O完成, 这里得到的dwKey就是 CreateIoCompletionPort 传进去的 CompletionKey
	// 这里的 lpol 是什么呢？
	BOOL bOK = ::GetQueuedCompletionStatus(ioCompletionPort,
			&dwTrans, (ULONG_PTR*)&dwKey, (LPOVERLAPPED*)&lpol, WSA_INFINITE);
	
	m_hAcceptEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

	// 注册FD_ACCEPT事件。
	// 如果投递的AcceptEx I/O不够，线程会接收到FD_ACCEPT网络事件，说明应该投递更多的AcceptEx I/O
	WSAEventSelect(m_sListen, m_hAcceptEvent, FD_ACCEPT);