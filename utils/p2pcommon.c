#include "p2pcommon.h"

////////////////////////////////////////////////////////////
//Mutex
////////////////////////////////////////////////////////////
void InitLock(THREAD_MUTEX_T* pLock)
{
#ifdef WIN32
	InitializeCriticalSection(pLock);
#else
	pthread_mutexattr_t mutexattr;
	pthread_mutexattr_init(&mutexattr);
#if !defined CM_SOLARIS && !defined MachOSupport 
	pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE_NP);
#else
	pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);
#endif
	pthread_mutex_init(pLock, &mutexattr);
	pthread_mutexattr_destroy(&mutexattr);
#endif
}

void UnInitLock(THREAD_MUTEX_T* pLock)
{
#ifdef WIN32
	DeleteCriticalSection(pLock);
#else
	pthread_mutex_destroy(pLock);
#endif // CM_WIN32
}

void Lock(THREAD_MUTEX_T* pLock)
{
#ifdef WIN32
	EnterCriticalSection(pLock);
#else
	pthread_mutex_lock(pLock);
#endif
}

void UnLock(THREAD_MUTEX_T* pLock)
{
#ifdef WIN32
	LeaveCriticalSection(pLock);
#else
	pthread_mutex_unlock(pLock);
#endif
}

////////////////////////////////////////////////////////////
//Log
////////////////////////////////////////////////////////////
THREAD_MUTEX_T g_LockLog;
FILE *g_hLogFile = NULL;

void InitLog(const char* lpszPath)
{
#if !defined ENABLE_P2PLOG
	return;
#else
	InitLock(&g_LockLog);
	g_hLogFile = fopen(lpszPath, "w+t");
#endif
}

void UnInitLog()
{
#if !defined ENABLE_P2PLOG
	return;
#else
	if(g_hLogFile != NULL)
	{
		fclose(g_hLogFile);
		g_hLogFile = NULL;
	}
	UnInitLock(&g_LockLog);
#endif
}
#if defined ENABLE_P2PLOG && !defined (ANDROID) && !defined (__IPHONE__)
void logMessage(int level, const char *format, ...)
{

	va_list ap;
	static char s_buffer[2048];
#ifdef WIN32
	SYSTEMTIME wtm;
#else
	struct timeval timeVal;
	struct tm tmVar;
#endif

	Lock(&g_LockLog);
	va_start(ap, format);
	if(g_hLogFile == NULL)
	{
		vprintf(format, ap);
		printf("\r\n");
	}
	else
	{
		vsprintf_s(s_buffer, 2047, format, ap);
#ifndef WIN32
		gettimeofday(&timeVal, NULL);
		localtime_r((const time_t*)&timeVal.tv_sec, &tmVar);
		fprintf(g_hLogFile, "[%02d/%02d/%04d %02d:%02d:%02d.%03lu pid=%d tid=%d] %s\r\n", 
			tmVar.tm_mon + 1, tmVar.tm_mday, tmVar.tm_year + 1900,
			tmVar.tm_hour, tmVar.tm_min, tmVar.tm_sec,
			timeVal.tv_usec / 1000,
			getpid(), (int)pthread_self(),
			s_buffer);
#else
		GetLocalTime(&wtm);
		fprintf(g_hLogFile, "[%02d/%02d/%04d %02d:%02d:%02d.%03d pid=%d tid=%d] %s\n", 
			wtm.wMonth, wtm.wDay, wtm.wYear, wtm.wHour, wtm.wMinute, 
			wtm.wSecond, wtm.wMilliseconds, GetCurrentProcessId(), 
			GetCurrentThreadId(), s_buffer);
#endif
		fflush(g_hLogFile);
	}
	va_end(ap);
	UnLock(&g_LockLog);
}
#endif
/////////////////////////////////////////////////////////////////////
//command line
/////////////////////////////////////////////////////////////////////
char* FindParamFromCmdLine(const char* key, int argc, char** argv)
{
	int i = 0;
	char* ret = NULL;

	for(i = 0; i < argc; i++)
	{
		printf((const char*)argv[i]);
		if(_stricmp(argv[i], key) == 0)
		{
			if(++i < argc && argv[i][0] != '-')
			{
				ret = argv[i];
			}
		}
	}
	return ret;
}

void ParseAddr(unsigned char* szIn, unsigned short* pPort, char* pIP, int nIPLen)
{
	char *szFind = strchr((char*)szIn, ':');
	int nAddrLen = 0;

	if(szIn == NULL || pPort == NULL || pIP == NULL)
		return;

	if(!szFind)
	{
		logMessage(logError, "ParseAddr, unknow aIpAddrAndPort=%s", szIn);
		*pPort = 0;
		szFind = (char*)szIn;
	}
	else 
	{
		*pPort = (unsigned short)atoi(szFind + 1);
	}

	nAddrLen = szFind - (char*)szIn;
	if(nAddrLen > 0)
	{
		memcpy_s(pIP, nIPLen, szIn, nAddrLen);
		pIP[nAddrLen] = '\0';
	}
}
