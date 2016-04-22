#include "stdafx.h"//这个文件里定义了 _WINSOCK_DEPRECATED_NO_WARNINGS 宏，不包含会报错
#include "CinitSock.h"

LRESULT CALLBACK WindowProc_WSAAsync(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_SOCKET:
	{
		SOCKET s = wParam;//有事件发生的套接字句柄
						  //查看是否出错
		if (WSAGETSELECTERROR(lParam))
		{
			::closesocket(s);
			return 0;
		}

		// 处理发生的事件
		switch (WSAGETSELECTEVENT(lParam))
		{
		case FD_ACCEPT://可以接受连接啦
		{
			SOCKET client = ::accept(s, NULL, NULL);
			::WSAAsyncSelect(client, hWnd, WM_SOCKET, FD_READ | FD_WRITE | FD_CLOSE);
		}
		break;
		case FD_WRITE:
		{}
		break;
		case FD_READ:
		{
			char buffer[maxBufferLen] = { 0 };
			if (::recv(s, buffer, sizeof(buffer), 0) == SOCKET_ERROR)
			{
				::closesocket(s);
			}
			else
			{
				printf("receive data:%s\n", buffer);
			}
		}
		break;
		case FD_CLOSE:
		{
			::closesocket(s);
		}
		break;
		default:
			break;
		}
	}
	return 0;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	default:
		break;
	}
	// 系统默认处理
	return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}


int do_wsaasync()
{
	//return do_select();
	wchar_t szClassName[] = L"MainWindow";
	wchar_t empty[] = L"";
	wchar_t errorString[] = L"创建窗口出错";
	WNDCLASSEX wndClass;
	wndClass.cbSize = sizeof(wndClass);
	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = WindowProc_WSAAsync;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = NULL;
	wndClass.hIcon = ::LoadIcon(NULL, IDI_APPLICATION);
	wndClass.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)::GetStockObject(WHITE_BRUSH);
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = szClassName;
	wndClass.hIconSm = NULL;
	::RegisterClassEx(&wndClass);

	// 创建主窗口
	HWND hWnd = ::CreateWindowEx(0,
		szClassName,
		empty,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL,
		NULL,
		NULL,
		NULL);

	if (!hWnd)
	{
		::MessageBox(NULL, errorString, errorString, MB_OK);
		return -1;
	}

	// 创建绑定listen
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
	// 设置套接字为 窗口接收类型
	::WSAAsyncSelect(sListen, hWnd, WM_SOCKET, FD_ACCEPT | FD_CLOSE);
	::listen(sListen, 5);
	// 从队列中取消息
	MSG msg;
	while (::GetMessage(&msg, NULL, 0, 0))
	{
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}

	return msg.wParam;//当 GetMessage 返回0时候结束
}