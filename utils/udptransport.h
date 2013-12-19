#if !defined(_UDP_TRANSPORT_H_INCLUDED_)
#define _UDP_TRANSPORT_H_INCLUDED_

#pragma once

#ifdef WIN32
#define USE_SELECT
#endif

#include "list.h"

#ifdef WIN32
#define WM_SOCKET_P2PUDP_SOCKET						WM_USER + 350
#define WM_WIN_CLASS_NAME_P2PUDP_SOCKET				"P2pUdpTransportClassName"
#endif
#define MAX_IP_ADDR_LEN   256

int P2PInit(void);
void P2PUnInit(void);

struct udp_transport;
typedef struct udp_transport UDPTRANSPORT;
typedef struct udp_transport *LPUDPTRANSPORT;

typedef struct udp_transport TCPTRANSPORT;
typedef struct udp_transport *LPTCPTRANSPORT;

typedef struct transport_sink
{
	void *pPtr;
	void (*OnConnect)(void* pPtr, LPUDPTRANSPORT trans, int nErrorCode);
    void (*OnReceive)(void* pPtr, LPUDPTRANSPORT trans, unsigned char* pbData, unsigned short nLen, unsigned long nPeerIP, unsigned short nPeerPort);
	void (*OnSend)(void* pPtr, LPUDPTRANSPORT trans, int nErrorCode);
	void (*OnDisconnect)(void* pPtr, LPUDPTRANSPORT trans, int nErrorCode);
}TRANSPORTSINK;

enum
{
	eTransportUDP,
	eTransportConnector,
	eTransportAcceptor,
	eTransportTCP,
};

struct udp_transport
{
	TRANSPORTSINK pSink;
	SOCKET s;
	char szBindAddr[MAX_IP_ADDR_LEN];
	unsigned short wBindPort;
	unsigned long dwBindFlags;
	int transType;
	unsigned long dwPeerIP;
	unsigned short wPeerPort;
	bool bSendErr;
#ifdef LINUX
	struct epoll_event	epoll_event;
#endif
	bool bConnectPending;
	bool bClosed;
};

#ifdef __cplusplus
extern "C" {
#endif

LPUDPTRANSPORT OpenUdpTransport(unsigned short nPort, TRANSPORTSINK sink, const char* localIP);

int SendUdpMessage(LPUDPTRANSPORT trans, char* buf, int len, const char* peerIP, unsigned short port);
int SendUdpData(LPUDPTRANSPORT trans, char* buf, int len, unsigned long dwPeerIP, unsigned short port);
void CloseUdpTransport(LPUDPTRANSPORT trans);

LPTCPTRANSPORT OpenTcpAcceptor(unsigned short nListenPort, TRANSPORTSINK sink, const char* localIP);
LPTCPTRANSPORT OpenTcpConnector(const char* svrIP, unsigned short nSvrPort, TRANSPORTSINK sink);
LPTCPTRANSPORT AcceptConnection(LPTCPTRANSPORT trans, TRANSPORTSINK sink);
int SendTcpData(LPTCPTRANSPORT trans, char* buf, int len);
void CloseTcpTransport(LPTCPTRANSPORT trans);
void SetNoDelay(LPTCPTRANSPORT trans, bool bNoDelay);
void NotifyHandler();

unsigned short GetBoundPort(LPUDPTRANSPORT trans, char* ip, int iplen);
void SetUdpBroadcast(LPUDPTRANSPORT trans, bool bBroadcast);
bool GetGatewayIp(char *gatewayip, int nsize);

#ifdef __cplusplus
}
#endif

#endif //!define _UDP_TRANSPORT_H_INCLUDED_
