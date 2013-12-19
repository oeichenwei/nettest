#include "p2pcommon.h"
#include "list.h"
#include "mytimer.h"
#include <signal.h>
#include "hashmap.h"
#include "udptransport.h"

void OnSvrConnect(void* pPtr, LPUDPTRANSPORT trans, int nErrorCode)
{
	logMessage(logWarning, "OnSvrConnect, never reach for UDP.");
}

void OnSvrSend(void* pPtr, LPUDPTRANSPORT trans, int nErrorCode)
{
}

void OnSvrDisconnect(void* pPtr, LPUDPTRANSPORT trans, int nErrorCode)
{
}

void OnSvrReceive(void* pPtr, 
				  LPUDPTRANSPORT trans, 
				  unsigned char* pbData, 
				  unsigned short nLen, 
				  unsigned long nPeerIP, 
				  unsigned short nPeerPort)
{
//	unsigned long dwNow = get_tick_count();

	if(pbData == NULL)
		return;
}

void setupUdpSink(TRANSPORTSINK pUdpSink, int bServer)
{
	pUdpSink.OnConnect = OnSvrConnect;
	pUdpSink.OnDisconnect = OnSvrDisconnect;
	pUdpSink.OnSend = OnSvrSend;
	pUdpSink.OnReceive = OnSvrReceive;
	pUdpSink.pPtr = (void*)bServer;
}

#ifndef WIN32
void sig_handler(int signum)
{
    logMessage(logInfo, "Receive signal. %d\n", signum);
}
#endif

void runloop()
{
#ifdef WIN32
	char szLine[1024];
	do
	{
		printf("Server is working, type exit to quit\r\n.");
		gets_s(szLine, 1024);
		if(strcmp(szLine, "exit") == 0)
			break;
	}while(1);
#else
	sigset_t oset;
    int sig;
	int rc = 0;

	signal(SIGTERM, SIG_IGN);
	signal(SIGINT, SIG_IGN);
    #ifdef SIGHUP
	signal(SIGHUP,SIG_IGN);
    #endif
    #ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
    #endif

	while (1)  
	{
        rc = sigwait(&oset, &sig);
        if (rc != -1) 
		{
            sig_handler(sig);
        }
		else 
		{
            logMessage(logInfo, "sigwaitinfo() returned err: %d; %s\n", errno, strerror(errno));
        }
    }
#endif
}

void runUdpServer(int udpPort)
{
	TRANSPORTSINK pUdpSink;
	LPUDPTRANSPORT pUdpSvr;
	logMessage(logInfo, "Run as UDP server, listen at port=%d", udpPort);

    setupUdpSink(pUdpSink, 1);
	pUdpSvr = OpenUdpTransport(udpPort, pUdpSink, NULL);

    runloop();
    
	CloseUdpTransport(pUdpSvr);
}

void runUdpClient(const char* szSvrIP, int nPort, int timeout, int nBandwidth)
{
	TRANSPORTSINK pUdpSink;
    LPUDPTRANSPORT pUdpClt;
    long lSleepTime = 0;
    int timeStart = get_tick_count();
    int timeOut = timeStart + timeout * 1000;

    setupUdpSink(pUdpSink, 0);
	logMessage(logInfo, "runUdpClient");

	pUdpClt = OpenUdpTransport(0, pUdpSink, NULL);
    lSleepTime = (long)(1000000.0 / (128 * nBandwidth));

    printf("udp client is running, packet interval is: %lu\r\n", lSleepTime);
    while(timeStart < timeOut)
    {
    	char szData[1024];

        SendUdpMessage(pUdpClt, szData, 1024, szSvrIP, nPort);
        timeStart = get_tick_count();
        usleep(lSleepTime);
    }
    printf("Client done.");
}

int main(int argc, char** argv)
{
    int c = 0;
    int nBandwidth = 1; //Units in Mbps.
    int nPort = 2008;
    int bUdp = 0;
    int bServer = 1;
    char* szSvrIP = "127.0.0.1";
    int nTimeOut = 10;
    
    while((c = getopt(argc, argv, "sc:p:ub:t:")) != -1)
    {
        switch (c)
        {
            case 's':
                bServer = 1;
                break;
            case 'b':
                nBandwidth = atoi(optarg);
                break;
            case 'c':
                szSvrIP = optarg;
                break;
            case 'p':
                nPort = atoi(optarg);
                break;
            case 'u':
                bUdp = 1;
                break;
            case 't':
                nTimeOut = atoi(optarg);
                break;
            default:
                printf("Invalid parameters to run netttest.");
                abort();
        }
    }

    printf("Netstat accepted parameters are: udp(%d), port(%d), is server(%d), connect to:(%s) and bandwidth is set to (%d)Mbps, timeout(%d)\r\n", bUdp, nPort, bServer, szSvrIP, nBandwidth, nTimeOut);
    
	P2PInit();
    if(bServer)
    {
        if(bUdp)
            runUdpServer(nPort);
        else
            printf("TODO\r\n");
    }
    else
    {
        if(bUdp)
            runUdpClient(szSvrIP, nPort, nTimeOut, nBandwidth);
    }
    
	P2PUnInit();
	return 0;
}

