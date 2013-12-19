#if !defined(_P2P_TRANSPORT_COMMON_H_INCLUDED_)
#define _P2P_TRANSPORT_COMMON_H_INCLUDED_

#pragma once

#ifdef WIN32

#include "winsock2.h"
#include "windows.h"
#include "time.h"

typedef int socklen_t;
typedef SOCKET Socket;
extern HINSTANCE g_hInstance;
typedef CRITICAL_SECTION THREAD_MUTEX_T;
typedef unsigned int size_t;

#ifndef __cplusplus
typedef char bool;
#endif

#ifndef true
#define true	1
#define false	0
#endif

#define EADDRINUSE		WSAEADDRINUSE
#define EADDRNOTAVAIL	WSAEADDRNOTAVAIL
#define ECONNREFUSED	WSAECONNREFUSED
#define ETIMEDOUT		WSAETIMEDOUT
#define ENOTSOCK		WSAENOTSOCK
#define ECONNRESET		WSAECONNRESET
#define EHOSTDOWN		WSAEHOSTDOWN
#define EHOSTUNREACH	WSAEHOSTUNREACH
#define EAFNOSUPPORT	WSAEAFNOSUPPORT
#define EWOULDBLOCK		WSAEWOULDBLOCK
#define EINPROGRESS		WSAEINPROGRESS
#define	ECONNABORTED	WSAECONNABORTED
#define INLINE			__inline

#else

#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#ifdef _LINUX_
#include <sys/epoll.h>
#include <error.h>
#endif
#include <sys/time.h>
#include <stdbool.h>
#include <netinet/tcp.h>
#include <netdb.h>

#define SOCKET int
#define Socket int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
typedef pthread_mutex_t THREAD_MUTEX_T;

#define strcpy_s(a,b,c) strncpy(a,c,b)
#define gets_s(a,b)	gets(a)
#define closesocket close
#define memcpy_s(a,b,c,d) memcpy(a, c, d)
#define sprintf_s snprintf
#define vsprintf_s vsnprintf
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#if !defined __IPHONE__
#define INLINE inline
#else
#define INLINE
#endif
#define SD_BOTH 2

#endif

#include "stdio.h"
#include <stdlib.h>
#include <errno.h>
#include "string.h"
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////
//Mutex
/////////////////////////////////////////////////
void InitLock(THREAD_MUTEX_T* pLock);
void Lock(THREAD_MUTEX_T* pLock);
void UnLock(THREAD_MUTEX_T* pLock);
void UnInitLock(THREAD_MUTEX_T* pLock);

/////////////////////////////////////////////////
//Logs
/////////////////////////////////////////////////
enum
{
	logDebug,
	logInfo,
	logWarning,
	logError
};

void InitLog(const char* lpszPath);
void UnInitLog();
#if defined ENABLE_P2PLOG
	#if defined ANDROID && defined ENABLE_ANDROIDLOG
		#include <utils/Log.h>
		#define logDebug ANDROID_LOG_DEBUG
		#define logWarning ANDROID_LOG_WARN
		#define logInfo ANDROID_LOG_INFO
		#define logError ANDROID_LOG_ERROR
		#define logMessage(LEVEL,...) __android_log_print(LEVEL,__FILE__,__VA_ARGS__)
	#elif defined __IPHONE__
		//#define logMessage(LEVEL,msg_fmt,args...) printf("[%s %d]" msg_fmt "\n",__FILE__,__LINE__,##args)
		#define logMessage(LEVEL,msg_fmt,args...) printf("[%d]" msg_fmt "\n",__LINE__,##args)
	#else
		void logMessage(int level, const char *format, ...);
	#endif
#else
	#define logMessage
#endif

/////////////////////////////////////////////////////////////////////
//command line
/////////////////////////////////////////////////////////////////////
char* FindParamFromCmdLine(const char* key, int argc, char** argv);

/////////////////////////////////////////////////////////////////////
//Parse address from format xx.xx.xx.xx:xxxx into IP and port
/////////////////////////////////////////////////////////////////////
void ParseAddr(unsigned char* szIn, unsigned short* pPort, char* pIP, int nIPLen);

#ifdef __cplusplus
}
#endif

#define P2P_PORT			17600
#define SERVER_CAPACITY		2048

#endif //!define _P2P_TRANSPORT_COMMON_H_INCLUDED_
