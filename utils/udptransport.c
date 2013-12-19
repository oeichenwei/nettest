#include "p2pcommon.h"
#include "mytimer.h"
#include "udptransport.h"

int g_bInited = 0;
List g_sockMap = NULL;
bool g_bMapDeleted = false;

//Used to notify when one new socket needs to be inserted into FD_Sets
//Or a new timer needs to be inserted into the timer que.
// 0 -- Listen socket; 1 -- Connect socket; 2 -- Accepted socket
LPTCPTRANSPORT g_notifier[3] = {NULL, NULL, NULL};

#ifdef WIN32
HWND g_hWndNotify = NULL;
HINSTANCE g_hInstance = NULL;
LRESULT CALLBACK UdpUtilWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI WndThreadProc(LPVOID lpParameter);
HANDLE g_hThread = NULL;
#endif

#ifdef SERVER
#define LOG_FILENAME	"logserver"
#else
#define LOG_FILENAME	"client"
#endif

#if defined LINUX || defined __IPHONE__
#include <pthread.h>

int g_epoll_fd = 0;
void* serv_epoll(void *p);
#define MAX_FDS		2048
#define EVENT_SIZE	65535
pthread_t g_hThread;

#endif

#if defined LINUX || defined __IPHONE__
#define GETERR()	err = errno;
#else
#define GETERR()	err = WSAGetLastError();
#endif

int RecvUdpMessage(Socket s, char* buf, int* len, unsigned long* ip, unsigned short* port);
void OnUDPInput(LPUDPTRANSPORT trans);
void OnTCPInput(LPUDPTRANSPORT trans);
void OnAcceptInput(LPUDPTRANSPORT trans);
void HandleRead(LPUDPTRANSPORT trans);
void SetSocketBuffer(LPUDPTRANSPORT trans);
void CreateNotifier();

int P2PInit()
{
#ifdef WIN32
	WNDCLASS wc = {0};
	WORD wVersionRequested = 0;
	WSADATA wsaData = {0};
	DWORD dwThreadId = 0;
	int err = 0;
#endif
	char szLogFilePath[1024];

	if(!g_bInited)
	{
		g_sockMap = MakeEmpty(NULL);
		InitTimerQue();

		//Start to initilize log
#ifdef WIN32
		sprintf_s(szLogFilePath, 1023, "%s_%d.log", LOG_FILENAME, GetCurrentProcessId());
#else
		sprintf_s(szLogFilePath, 1023, "%s_%d.log", LOG_FILENAME, getpid());
#endif
		InitLog(szLogFilePath);

		//Start to initilize the socket library
#ifdef WIN32
		wVersionRequested = MAKEWORD(2, 2);
		err = WSAStartup(wVersionRequested, &wsaData);
		if(err != 0)
			return 0;
	#ifndef USE_SELECT
		g_hInstance = (HINSTANCE)GetModuleHandle(NULL);
		wc.style = 0;
		wc.lpfnWndProc = (WNDPROC)UdpUtilWndProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = sizeof(void*);
		wc.hInstance = g_hInstance;
		wc.hIcon = 0;
		wc.hCursor = 0;
		wc.hbrBackground = 0;
		wc.lpszMenuName = NULL;
		wc.lpszClassName = WM_WIN_CLASS_NAME_P2PUDP_SOCKET;
		RegisterClass(&wc);
		
		g_hWndNotify = CreateWindow(WM_WIN_CLASS_NAME_P2PUDP_SOCKET, NULL, WS_OVERLAPPED, 0, 
			0, 0, 0, NULL, NULL, 0, 0);
		if(NULL == g_hWndNotify)
			return 0;
	#endif
#else
	#ifndef USE_SELECT
		g_epoll_fd = epoll_create(MAX_FDS);
	#endif
#endif

		//Create notifier
		CreateNotifier();

		//Start to create the network thread
		g_bInited = 1;
#if defined(WIN32) && defined(USE_SELECT)
		g_hThread = CreateThread(NULL, 0, WndThreadProc, 0, 0, &dwThreadId);
#else
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
		if(pthread_create(&g_hThread, &attr, serv_epoll, NULL) != 0)
		{
			logMessage(logError, "Create network thread failed.");
			return 0;
		}
#endif
		logMessage(logInfo, "P2PInit, done.");
	}
	
	return g_bInited;
}

void JoinNetThread()
{
#ifdef WIN32
	WaitForSingleObject(g_hThread, INFINITE);
#else
	pthread_join(g_hThread, NULL);
#endif
}

void P2PUnInit()
{
	logMessage(logInfo, "P2PUnInit, start");
	if(g_bInited)
	{
		g_bInited = 0;
		NotifyHandler();

		//Should wait for the thread to exit!
		JoinNetThread();

#ifdef WIN32
		DeleteList(g_sockMap);
		g_sockMap = NULL;

		UnInitTimerQue();

		if(g_hWndNotify && IsWindow(g_hWndNotify))
		{
			DestroyWindow(g_hWndNotify);
			g_hWndNotify = NULL;
		}
		UnregisterClass(WM_WIN_CLASS_NAME_P2PUDP_SOCKET, g_hInstance);
		WSACleanup();
#endif
#if defined LINUX || defined __IPHONE__
		if(g_epoll_fd)
		{
			close(g_epoll_fd);
			g_epoll_fd = 0;
		}
#endif
		logMessage(logInfo, "P2PUnInit, start");

		UnInitLog();
	}
}

int FillFdSets(fd_set* fsRead, fd_set* fsWrite, fd_set* fsException)
{
	int nMaxFd = -1;
	Position it;

	if(g_sockMap == NULL)
		return -1;
	LockList(g_sockMap);
	it = First(g_sockMap);
	while(it != NULL)
	{
		LPUDPTRANSPORT trans = (LPUDPTRANSPORT)Retrieve(it);

		if(trans->bClosed && trans->transType == eTransportTCP)
		{
			it = Advance(it);
			continue;
		}

		FD_SET(trans->s, fsRead);
		if(trans->bSendErr)
		{
			FD_SET(trans->s, fsWrite);
		}

		FD_SET(trans->s, fsException);
		if(nMaxFd < (int)trans->s)
			nMaxFd = trans->s;
		it = Advance(it);
	}
	UnLockList(g_sockMap);

	return nMaxFd;
}

void OnUDPInput(LPUDPTRANSPORT trans)
{
	static char s_buffer[2048] = {0};
	static int s_len = 0;
	unsigned long dwPeerIP;
	unsigned short wPeerPort;

	if(trans == NULL)
		return;

	s_len = 2048;
	RecvUdpMessage(trans->s, s_buffer, &s_len, &dwPeerIP, &wPeerPort);
//	logMessage(logDebug, "Received: %s, len=%d, peerip=%d, peer port=%d", s_buffer, s_len, dwPeerIP, wPeerPort);
	if(trans->pSink.pPtr)
		trans->pSink.OnReceive(trans->pSink.pPtr, trans, (unsigned char*)s_buffer, s_len, dwPeerIP, wPeerPort);
}

void OnAcceptInput(LPUDPTRANSPORT trans)
{
	if(trans->pSink.pPtr)
		trans->pSink.OnConnect(trans->pSink.pPtr, trans, 0);
}

#if defined LINUX || defined __IPHONE__
void setnonblocking(int fd)
{
	int opts;
	opts = fcntl(fd, F_GETFL);
	if (opts < 0)
	{
		return;
	}
	opts = opts | O_NONBLOCK;
	if(fcntl(fd, F_SETFL, opts) < 0)
	{
		return;
	}
	return;
}
#endif

void RegistHandler(LPUDPTRANSPORT trans)
{
#ifdef WIN32
	u_long nonblock = 1;
	if(g_sockMap != NULL)
	{
		LockList(g_sockMap);
		Insert(trans, g_sockMap, Header(g_sockMap));
		UnLockList(g_sockMap);
	}
#ifndef USE_SELECT
	WSAAsyncSelect(trans->s, g_hWndNotify, WM_SOCKET_P2PUDP_SOCKET, FD_READ | FD_WRITE );
#else
	ioctlsocket(trans->s, FIONBIO, &nonblock);
#endif
#elif defined(LINUX) || defined __IPHONE__
	setnonblocking(trans->s);
#ifdef USE_SELECT
	if(g_sockMap != NULL)
	{
		LockList(g_sockMap);
		Insert(trans, g_sockMap, Header(g_sockMap));
		UnLockList(g_sockMap);
	}
#else
	trans->epoll_event.data.fd = trans->s;
	trans->epoll_event.data.ptr = trans;
	//Don't use ET for UDP and acceptor!
	trans->epoll_event.events = EPOLLIN/* | EPOLLET*/;
	if(trans->transType == eTransportTCP || trans->transType == eTransportConnector)
		trans->epoll_event.events |= EPOLLET;

	epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, trans->s, &trans->epoll_event);
#endif
#endif
	NotifyHandler();
}

LPTCPTRANSPORT AcceptConnection(LPTCPTRANSPORT trans, TRANSPORTSINK sink)
{
	struct sockaddr_in from;
	socklen_t fromLen = sizeof(from);
	TCPTRANSPORT *newtrans = NULL;

	do
	{
		newtrans = (TCPTRANSPORT*)malloc(sizeof(TCPTRANSPORT));
		if(newtrans == NULL)
			break;

		memset(newtrans, 0, sizeof(TCPTRANSPORT));
		newtrans->s = accept(trans->s, (struct sockaddr *)&from, &fromLen);
		newtrans->dwPeerIP = from.sin_addr.s_addr;
		newtrans->wPeerPort = ntohs(from.sin_port);
		newtrans->transType = eTransportTCP;
		newtrans->pSink = sink;

		SetSocketBuffer(newtrans);
		SetNoDelay(newtrans, true);

		RegistHandler(newtrans);
	}
	while(false);

	return newtrans;
}

void HandleRead(LPUDPTRANSPORT trans)
{
	if(trans->transType == eTransportUDP)
		OnUDPInput(trans);
	else if(trans->transType == eTransportTCP)
		OnTCPInput(trans);
	else if(trans->transType == eTransportAcceptor)
		OnAcceptInput(trans);
	else if(trans->transType == eTransportConnector)
	{
		if(trans->bConnectPending)
		{
			SetSocketBuffer(trans);
			SetNoDelay(trans, true);
			if(trans->pSink.pPtr != NULL)
				trans->pSink.OnConnect(trans->pSink.pPtr, trans, 0);
			trans->transType = eTransportTCP;
		}
	}
	else
	{
		assert(false);
		logMessage(logError, "HandleRead, unknown transport type.");
	}
}

void HandleWrite(LPUDPTRANSPORT trans)
{
	if(trans == NULL)
		return;

	if(trans->transType == eTransportConnector)
	{
		SetSocketBuffer(trans);
		SetNoDelay(trans, true);
		if(trans->pSink.pPtr != NULL)
			trans->pSink.OnConnect(trans->pSink.pPtr, trans, 0);
		trans->transType = eTransportTCP;
	}
	else
	{
		if(trans->pSink.pPtr)
			trans->pSink.OnSend(trans->pSink.pPtr, trans, 0);
		trans->bSendErr = false;
	}
}

void ProcessFdSets(fd_set* fsRead, fd_set* fsWrite, fd_set* fsException, int nSelect, int nMaxFd)
{
	Position it;

	if(g_sockMap == NULL)
		return;

	g_bMapDeleted = false;
	LockList(g_sockMap);
	it = First(g_sockMap);

	while(it != NULL && nSelect > 0 && !g_bMapDeleted)
	{
		LPUDPTRANSPORT trans = (LPUDPTRANSPORT)Retrieve(it);
		it = Advance(it);
		if((int)trans->s > nMaxFd)
		{
			logMessage(logInfo, "New socket added into the list.");
			it = Advance(it);
			continue;
		}

		if(FD_ISSET(trans->s, fsRead))
		{
			HandleRead(trans);
			nSelect--;
		}
		if(g_bMapDeleted)
			break;

		if(trans->bSendErr && FD_ISSET(trans->s, fsWrite))
		{
			HandleWrite(trans);
			nSelect--;
		}
		if(g_bMapDeleted)
			break;

		if(FD_ISSET(trans->s, fsException))
		{
			trans->bClosed = true;
			if(trans->pSink.pPtr)
				trans->pSink.OnDisconnect(trans->pSink.pPtr, trans, 0);
			nSelect--;
		}
	}
	UnLockList(g_sockMap);
}

void SelectProc()
{
	struct timeval tvSelect;
	fd_set fsRead, fsWrite, fsException;
	int nMaxFd;
	int nSelect;
	long delaytime;

	logMessage(logInfo, "Using select.");
	while(g_bInited)
	{
		delaytime = ProcessTimer();

		tvSelect.tv_sec = 0;
		if(delaytime == -1)
		{
			tvSelect.tv_sec = 3;
			tvSelect.tv_usec = 0;
		}
		else
		{
			tvSelect.tv_sec = delaytime / 1000;
			tvSelect.tv_usec = (delaytime % 1000) * 1000;
		}

		FD_ZERO(&fsRead);
		FD_ZERO(&fsWrite);
		FD_ZERO(&fsException);

		if(g_sockMap == NULL)
			break;

		nMaxFd = FillFdSets(&fsRead, &fsWrite, &fsException);

#ifdef CM_MACOS
#ifndef MachOSupport
		nSelect = CFM_select(nMaxFd+1, &fsRead, &fsWrite, &fsException, &tvSelect);
		if(nSelect == 0 || (nSelect == -1 && CFM_geterrno() == EINTR))
#else
		nSelect = select(nMaxFd+1, &fsRead, &fsWrite, &fsException, &tvSelect);
		if(nSelect == 0 || (nSelect == -1 && errno == EINTR))
#endif	//MachOSupport  	
#else
		nSelect = select(nMaxFd+1, &fsRead, &fsWrite, &fsException, &tvSelect);
		if (nSelect == 0 || (nSelect == -1 && errno == EINTR))
#endif	
			continue;
		else if (nSelect == -1) 
		{
			if(nMaxFd > 0)
			{
				logMessage(logError, "CCmReactorSelect::RunEventLoop, select() failed! maxfd=%d, err=%d", nMaxFd, errno);
				return;
			}
#ifdef WIN32
			Sleep(30);
#else
			usleep(30000);
#endif
			continue;
		}

		if(!g_bInited)
			break;

		ProcessFdSets(&fsRead, &fsWrite, &fsException, nSelect, nMaxFd);
	}
}

#ifdef WIN32
UDPTRANSPORT* GetFromSocketMap(SOCKET s)
{
	UDPTRANSPORT* trans = NULL;
	Position pos;
	LockList(g_sockMap);
	pos = First(g_sockMap);
	while(pos != NULL)
	{
		trans = (UDPTRANSPORT*)pos->Element;
		if(trans->s == s)
		{
			UnLockList(g_sockMap);
			return trans;
		}
	}
	UnLockList(g_sockMap);
	return NULL;
}

void DoSocketCallback(UDPTRANSPORT* trans, LPARAM lParam)
{
	int nErrCode = WSAGETSELECTERROR(lParam);
	int nEventCode = WSAGETSELECTEVENT(lParam);

	if(trans == NULL)
		return;

	switch (nEventCode)
	{
	case FD_CONNECT:
	case FD_READ:
		HandleRead(trans);
		break;
	case FD_WRITE:
		if(trans->pSink.pPtr)
			trans->pSink.OnSend(trans->pSink.pPtr, trans, nErrCode);
		break;
	case FD_CLOSE:
		trans->bClosed = true;
		if(trans->pSink.pPtr)
			trans->pSink.OnDisconnect(trans->pSink.pPtr, trans, nErrCode);
		break;
	}
}

LRESULT CALLBACK UdpUtilWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if(uMsg == WM_SOCKET_P2PUDP_SOCKET)
	{
		UDPTRANSPORT* trans = GetFromSocketMap(wParam);
		if(trans)
		{
			DoSocketCallback(trans, lParam);
		}
		else
		{
			logMessage(logInfo, "UdpUtilWndProc, can not found CUdpUtil, Msg=%d", uMsg);
		}
		return 0L;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

DWORD WINAPI WndThreadProc(LPVOID lpParameter)
{
	SelectProc();
	return 0;
}
#endif

LPUDPTRANSPORT CreateTransport(int eTransType, TRANSPORTSINK sink, unsigned short nPort, const char* localIP)
{
	int err = -1;
	struct sockaddr_in addr;
	unsigned long dwIP = 0;
	int nOne = 1;
	UDPTRANSPORT *trans = NULL;

	trans = (UDPTRANSPORT*)malloc(sizeof(struct udp_transport));
	logMessage(logInfo, "CreateTransport, type=%d, nPort=%d, localIP=%s, trans=0x%x", eTransType, nPort, localIP ? localIP : "0.0.0.0", trans);
	if(trans == NULL)
		return NULL;

	memset(trans, 0, sizeof(struct udp_transport));
	trans->pSink = sink;
	if(localIP != NULL)
	{
		strcpy_s(trans->szBindAddr, MAX_IP_ADDR_LEN, localIP);
		dwIP = inet_addr(localIP);
	}
	else
		trans->szBindAddr[0] = 0;
	trans->wBindPort = nPort;
	trans->dwBindFlags = 0;
	trans->bSendErr = false;

	do
	{
		trans->transType = eTransType;
		if(eTransType == eTransportUDP)
		{
			trans->s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
		}
		else
		{
			trans->s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		}

		if(trans->s == INVALID_SOCKET)
		{
			GETERR();
			logMessage(logError, "Failed to create socket, %d", err);
			break;
		}

		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(nPort);

		if((dwIP != 0) && ( dwIP != 0x100007f )) //Broadcast address is not allowed
		{
			addr.sin_addr.s_addr = dwIP;
		}

		if(setsockopt(trans->s, SOL_SOCKET,SO_REUSEADDR,(char *)&nOne,sizeof(nOne)) == SOCKET_ERROR)
		{
			GETERR();
			logMessage(logError, "Failed to setsockopt SO_REUSEADDR error, err=%d", err);
		}

		if(eTransType != eTransportConnector)
		{
			if(bind(trans->s, (struct sockaddr*)&addr, sizeof(addr)) != 0)
			{
				GETERR();
				logMessage(logError, "Could not bind UDP receive port due to system error, errorCode=%d", err);
				switch(err)
				{
				case 0:
					{
						logMessage(logError, "Could not bind socket" );
						break;
					}
				case EADDRINUSE:
					{
						logMessage(logError, "Port for receiving UDP is in use");
						break;
					}
				case EADDRNOTAVAIL:
					{
						logMessage(logError, "Cannot assign requested address");
						break;
					}
				default:
					break;
				}
				break;
			}
		}

		RegistHandler(trans);
		err = 0;
	}
	while(0);

	if(err != 0)
	{
		free(trans);
		trans = NULL;
	}

	return trans;
}

LPUDPTRANSPORT OpenUdpTransport(unsigned short nPort, TRANSPORTSINK sink, const char* localIP)
{
	LPUDPTRANSPORT trans = NULL;

	trans = CreateTransport(eTransportUDP, sink, nPort, localIP);
	SetSocketBuffer(trans);

	return trans;
}

int RecvUdpMessage(Socket s, char* buf, int* len,  unsigned long* ip, unsigned short* port)
{
	int nSize = *len;
	int err = 0;
	struct sockaddr_in from;
	socklen_t fromLen = sizeof(from);

	if(s == INVALID_SOCKET || *len <= 0)
		return 0;

//	logMessage(logDebug, "RecvUdpMessage");
	memset(&from, 0, fromLen);
	*len = recvfrom(s, buf, nSize, 0, (struct sockaddr *)&from, (socklen_t*)&fromLen);
	if(*len == SOCKET_ERROR || *len <= 0)
	{
		GETERR();
		logMessage(logError, "Failed to recv UDP message, err=%d, len=%d", err, *len);
		switch(err)
		{
		case ENOTSOCK:
			break;
		case ECONNRESET:
			break;
		default:
//			logMessage(logError, "Failed to recv UDP message");
			break;
		}
		return 0;
	}
	*port = ntohs(from.sin_port);
	*ip = from.sin_addr.s_addr;
	if((*len)+1 >= nSize)
	{
		logMessage(logError, "Received a message that was too large");
		return 0;
	}

	buf[*len]=0;
	return 1;
}

void OnTCPInput(LPUDPTRANSPORT trans)
{
	static char s_buffer[64*1024] = {0};
	static int s_len = 0;
	int nCount = 4;
	int nRet = 0;
	int err = -1;

	while(--nCount)
	{
		s_len = 64*1024;
		nRet = recv(trans->s, s_buffer, s_len, 0);
		if(nRet == SOCKET_ERROR) 
		{
			GETERR();
			nRet = -1;
			if(err != EWOULDBLOCK)
				logMessage(logError, "OnTCPInput, recv error = %d", err);
			if(err == ECONNRESET || ECONNABORTED == err)
			{
				trans->bClosed = true;
				if(trans->pSink.pPtr != NULL)
					trans->pSink.OnDisconnect(trans->pSink.pPtr, trans, err);
			}
			return;
		}
		//Remote has called shutdown to the socket.
		if(nRet == 0)
		{
			trans->bClosed = true;
			if(trans->pSink.pPtr != NULL)
				trans->pSink.OnDisconnect(trans->pSink.pPtr, trans, 0);
			return;
		}

		if(trans->pSink.pPtr != NULL)
			trans->pSink.OnReceive(trans->pSink.pPtr, trans, (unsigned char*)s_buffer, nRet, trans->dwPeerIP, trans->wPeerPort);

		if (nRet < 64*1024)
			return;
	}
}

int SendTcpData(LPTCPTRANSPORT trans, char* buf, int len)
{
	int nRet = 0;
	if(trans == NULL)
		return -1;

	nRet = send(trans->s, buf, len, 0);
	if(nRet == SOCKET_ERROR)

		trans->bSendErr = true;

	return nRet;
}

int SendUdpData(LPUDPTRANSPORT trans, char* buf, int len, unsigned long dwPeerIP, unsigned short port)
{
	int nRetVal;
	int err = 0;
	struct sockaddr_in to = {0};
	socklen_t toLen = sizeof(to);

	if(trans == NULL || trans->s == INVALID_SOCKET)
		return -1;

	memset(&to, 0, toLen);
	to.sin_family = AF_INET;
	to.sin_port = htons(port);
	to.sin_addr.s_addr = dwPeerIP;

	nRetVal = sendto(trans->s, buf, len, 0, (struct sockaddr*)&to, toLen);
	if(nRetVal == SOCKET_ERROR)
	{
		GETERR();
		logMessage(logError, "Send UDP err = %d", err);
		switch (err)
		{
		case ECONNREFUSED:
		case EHOSTDOWN:
		case EHOSTUNREACH:
		case EAFNOSUPPORT:
			break;
		default:
			break;
		}
		return -1;
	}

	if(nRetVal == 0 || nRetVal != len)
	{
		logMessage(logError, "no data sent in send, retval=%d, len=%d", nRetVal, len);
		return -2;
	}

	return nRetVal;
}

int SendUdpMessage(LPUDPTRANSPORT trans, char* buf, int len, const char* peerIP, unsigned short port)
{
	if(peerIP == NULL)
		return -1;
	return SendUdpData(trans, buf, len, inet_addr(peerIP), port);
}

void CloseUdpTransport(LPUDPTRANSPORT trans)
{
	if(trans != NULL && INVALID_SOCKET != trans->s)
	{
		trans->pSink.pPtr = NULL;
#ifdef WIN32
		LockList(g_sockMap);
		Delete(trans, g_sockMap);
		g_bMapDeleted = true;
		UnLockList(g_sockMap);
	#ifndef USE_SELECT
		WSAAsyncSelect(trans->s, g_hWndNotify, 0, 0);
	#endif
#elif defined(LINUX) || defined __IPHONE__
	#ifdef USE_SELECT
		LockList(g_sockMap);
		Delete(trans, g_sockMap);
		g_bMapDeleted = true;
		UnLockList(g_sockMap);
	#else
		epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, trans->s, NULL);
	#endif
#endif
		closesocket(trans->s);
		trans->s = INVALID_SOCKET;
		free(trans);
	}
}

LPTCPTRANSPORT OpenTcpAcceptor(unsigned short nListenPort, TRANSPORTSINK sink, const char* localIP)
{
	LPTCPTRANSPORT trans = NULL;
	int err = -1;

	trans = CreateTransport(eTransportAcceptor, sink, nListenPort, localIP);
	if(trans != NULL)
	{
		if(listen(trans->s, SOMAXCONN) == SOCKET_ERROR)
		{
			GETERR();
			assert(false);
			logMessage(logInfo, "OpenTcpAcceptor, listen error = %d.", err);
			CloseTcpTransport(trans);
			free(trans);
			return NULL;
		}
	}
	return trans;
}

LPTCPTRANSPORT OpenTcpConnector(const char* svrIP, unsigned short nSvrPort, TRANSPORTSINK sink)
{
	LPTCPTRANSPORT trans = NULL;
	int err = -1;
	struct sockaddr_in to = {0};
	socklen_t toLen = sizeof(to);

	if(svrIP == NULL || nSvrPort == 0)
	{
		assert(false);
		logMessage(logError, "OpenTcpConnector, invalid parameters.");
		return NULL;
	}

	trans = CreateTransport(eTransportConnector, sink, 0, NULL);
	if(trans == NULL)
		return NULL;

	memset(&to, 0, toLen);
	to.sin_family = AF_INET;
	to.sin_port = htons(nSvrPort);
	to.sin_addr.s_addr = inet_addr(svrIP);

	trans->bConnectPending = false;
	if(connect(trans->s, (struct sockaddr*)&to, toLen) == SOCKET_ERROR)
	{
		GETERR();
		logMessage(logError, "OpenTcpConnector, connect error, err = %d, this=0x%x", err, trans);
		trans->bConnectPending = true;
		if(err != EWOULDBLOCK && err != EINPROGRESS)
		{
			assert(false);
			CloseTcpTransport(trans);
			free(trans);
			return NULL;
		}
	}
	trans->bSendErr = true;
	return trans;
}

void CloseTcpTransport(LPTCPTRANSPORT trans)
{
	if(trans != NULL)
		shutdown(trans->s, SD_BOTH);
	CloseUdpTransport(trans);
}

void SetNoDelay(LPTCPTRANSPORT trans, bool bNoDelay)
{
	int nNoDelay = bNoDelay ? 1 : 0;

	if(trans != NULL)
		setsockopt(trans->s, IPPROTO_TCP, TCP_NODELAY, (const char*)&nNoDelay, sizeof(nNoDelay));
}

void SetSocketBuffer(LPUDPTRANSPORT trans)
{
	unsigned long dwRcv = 65535;
	unsigned long dwSnd = 65535;
	int level = trans->transType == eTransportUDP ? SOL_SOCKET : IPPROTO_TCP;

	if(trans != NULL)
	{
		setsockopt(trans->s, level, SO_SNDBUF, (const char*)&dwSnd, sizeof(dwSnd));
		setsockopt(trans->s, level, SO_RCVBUF, (const char*)&dwRcv, sizeof(dwRcv));
	}
}

void OnNotifierReceive(void* pPtr, LPUDPTRANSPORT trans, unsigned char* pbData, unsigned short nLen, unsigned long nPeerIP, unsigned short nPeerPort)
{
//	logMessage(logInfo, "OnNotifierReceive, pPtr=%d, contents=%s", pPtr, pbData);
}

void OnNotifierSend(void* pPtr, LPUDPTRANSPORT trans, int nErrorCode)
{
	logMessage(logInfo, "OnNotifierSend, pPtr=%d", pPtr);
}

void OnNotifierDisconnect(void* pPtr, LPUDPTRANSPORT trans, int nErrorCode)
{
	logMessage(logInfo, "OnNotifierDisconnect, pPtr=%d", pPtr);
}

void OnNotifierConnect(void* pPtr, LPUDPTRANSPORT trans, int nErrorCode)
{
	TRANSPORTSINK sink = {(void*)1030, OnNotifierConnect, OnNotifierReceive, OnNotifierSend, OnNotifierDisconnect};
	logMessage(logInfo, "OnNotifierConnect, pPtr=%d", pPtr);
	if((void*)1217 == pPtr)
		g_notifier[2] = AcceptConnection(trans, sink);
}

void SetUdpBroadcast(LPUDPTRANSPORT trans, bool bBroadcast)
{
    int bOptLen = sizeof(bBroadcast);

	if(trans != NULL)
		setsockopt(trans->s, SOL_SOCKET, SO_BROADCAST, (char*)&bBroadcast, bOptLen); 
}

unsigned short GetBoundPort(LPUDPTRANSPORT trans, char* ip, int iplen)
{
	struct sockaddr_in myAddr = {0};
	socklen_t myAddrLen = sizeof(myAddr);
	int err = 0;

	if(trans == NULL)
		return 0;

	if(getsockname(trans->s, (struct sockaddr *)&myAddr, &myAddrLen) == SOCKET_ERROR)
	{
		GETERR();
		logMessage(logError, "GetBoundPort, error=%d", err);
		return 0;
	}

	if(ip != NULL && iplen > 0)
	{
		char* szIP = inet_ntoa(myAddr.sin_addr);
		strcpy_s(ip, iplen - 1, szIP);
	}
	return ntohs(myAddr.sin_port);
}

void CreateNotifier()
{
	unsigned short nBindedPort = 0;

	TRANSPORTSINK sink = {(void*)1217, OnNotifierConnect, OnNotifierReceive, OnNotifierSend, OnNotifierDisconnect};

	if(g_notifier[0] != NULL)
		return;

	g_notifier[0] = OpenTcpAcceptor(0, sink, "127.0.0.1");
	if(g_notifier[0] == NULL)
		return;

	nBindedPort = GetBoundPort(g_notifier[0], NULL, 0);
	if(nBindedPort > 0)
	{
		logMessage(logInfo, "CreateNotifier, port=%d", nBindedPort);
		sink.pPtr = (void*)419;
		g_notifier[1] = OpenTcpConnector("127.0.0.1", nBindedPort, sink);
	}
}

void NotifyHandler()
{
	static char* szMessage = "Born@1982";
	static int nLen = 10;

	if(g_notifier[1] != NULL)
		SendTcpData(g_notifier[1], szMessage, nLen);
}

#if defined LINUX 
void OnEpollEvents(LPUDPTRANSPORT trans, unsigned long events)
{
	if(trans == NULL)
		return;

//	logMessage(logDebug, "OnEpollEvents, event = %d, this=0x%x", events, trans);
	if(events & EPOLLIN)
	{
		HandleRead(trans);
	}
	else if(events & EPOLLOUT)
	{
		HandleWrite(trans);
	}
	else if(events & EPOLLERR)
	{
		trans->bClosed = true;
		if(trans->pSink.pPtr)
			trans->pSink.OnDisconnect(trans->pSink.pPtr, trans, 0);
	}
	else
	{
		trans->bClosed = true;
		if(trans->pSink.pPtr)
			trans->pSink.OnDisconnect(trans->pSink.pPtr, trans, -1);
	}
}
#endif
#if defined LINUX || defined __IPHONE__
void* serv_epoll(void *p)
{
#ifdef USE_SELECT
	SelectProc();
#else
	int i, cfd, nfds;
	LPUDPTRANSPORT trans = NULL;
	struct epoll_event events[EVENT_SIZE];
	int timeout;
	long delaytime;

	logMessage(logDebug, "serv_epoll enter");
	while(g_bInited)
	{
		timeout = 30000;
		delaytime = ProcessTimer();
		if(delaytime > 0)
			timeout = delaytime;

		nfds = epoll_wait(g_epoll_fd, events, EVENT_SIZE , timeout);
		if(!g_bInited)
			break;
		if(nfds < 0) 
		{
			if(errno != EINTR)
			{
				logMessage(logInfo, "CUdpUtil::serv_epoll, err=%d", errno);
			}
			else
				continue;
		} 
		else if(nfds > 0) 
		{
//			logMessage(logDebug, "event received, nfds = %d.", nfds);
			for(i = 0; i < nfds; i++)
			{
//				logMessage(logDebug, "Event ptr=%x, events=%d", events[i].data.ptr, events[i].events);
				trans = (LPUDPTRANSPORT)events[i].data.ptr;
				cfd = events[i].data.fd;
				if(NULL == trans)
				{
					logMessage(logWarning, "trans == NULL, continued");
					continue;
				}
				OnEpollEvents(trans, events[i].events);
			}
		}
	}
	logMessage(logDebug, "serv_epoll exit");
#endif

	return NULL;
}
#endif
