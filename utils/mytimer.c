#include "p2pcommon.h"
#include "mytimer.h"
#include "list.h"
#include "udptransport.h"

List g_timer = NULL;
unsigned long g_ulTimerSeq = 419;

void InitTimerQue()
{
	g_timer = MakeEmpty(NULL);
}

void UnInitTimerQue()
{
	DeleteList(g_timer);
	g_timer = NULL;
}

Position Rearrange(Position itPrevOld, Position itOld)
{
	Position it, itPrev;
	time_node* pTemp;
	time_node* pTimerNode;

	pTimerNode = (time_node*)Retrieve(itOld);
	itPrev = itOld;
	it = Advance(itOld);
	while(it != NULL)
	{
		pTemp = (time_node*)Retrieve(it);
		if(pTemp->ulNextTime > pTimerNode->ulNextTime)
			break;
		itPrev = it;
		it = Advance(it);
	}

	if(itPrev != itOld)
	{
		itPrevOld->Next = itOld->Next;
		itOld->Next = it;
		itPrev->Next = itOld;
		return itPrevOld->Next;
	}
	return itOld;
}

long ProcessTimer()
{
	Position it;
	Position itPrev;
	struct time_node* tvTimer;
	unsigned long ulNow;
	long ulRet = -1;

	ulNow = get_tick_count();
	if(g_timer == NULL)
		return -1;

	LockList(g_timer);
	it = First(g_timer);
	itPrev = Header(g_timer);
	while(it != NULL)
	{
		tvTimer = (struct time_node*)Retrieve(it);
		if(ulNow >= tvTimer->ulNextTime)
		{
			tvTimer->OnTimer(tvTimer->pPtr, tvTimer->ulID);
			if(itPrev->Next != it)
			{
				it = itPrev->Next;
				continue;
			}
			if(tvTimer->nRemainCount == 0)
			{
				free(tvTimer);
				it = Advance(it);
				itPrev->Next = it;
			}
			else
			{
				if(tvTimer->nRemainCount > 0)
					tvTimer->nRemainCount--;
				tvTimer->ulNextTime += tvTimer->nInterval;
				it = Rearrange(itPrev, it);
			}
		}
		else
		{
			ulRet = tvTimer->ulNextTime - ulNow;
			break;
		}
	}
	UnLockList(g_timer);

	return ulRet;
}

unsigned long RegisterTimer(int milisecond, P2PTimerProc callback, void* pParam, int nRepeatTime)
{
	struct time_node* pTimerNode;
	struct time_node* pTemp;
	Position it, itPrev;

	pTimerNode = (struct time_node*)malloc(sizeof(struct time_node));
	pTimerNode->pPtr = pParam;
	pTimerNode->OnTimer = callback;
	pTimerNode->ulNextTime = get_tick_count() + milisecond;
	pTimerNode->nRemainCount = nRepeatTime;
	pTimerNode->ulID = g_ulTimerSeq++;
	pTimerNode->nInterval = milisecond;

	LockList(g_timer);
	it = First(g_timer);
	itPrev = Header(g_timer);
	while(it != NULL)
	{
		pTemp = (struct time_node*)Retrieve(it);
		if(pTemp->ulNextTime > pTimerNode->ulNextTime)
			break;
		itPrev = it;
		it = Advance(it);
	}
	Insert(pTimerNode, g_timer, itPrev);
	UnLockList(g_timer);

	NotifyHandler();

	return pTimerNode->ulID;
}

void CancelTimer(unsigned long ulTimerID)
{
	Position it, itPrev;
	struct time_node* pTemp;

	LockList(g_timer);
	it = First(g_timer);
	itPrev = Header(g_timer);
	while(it != NULL)
	{
		pTemp = (struct time_node*)Retrieve(it);
		if(pTemp->ulID == ulTimerID)
		{
			it = Advance(it);
			itPrev->Next = it;
			free(pTemp);
			break;
		}
		itPrev = it;
		it = Advance(it);
	}

	UnLockList(g_timer);
}

#ifdef WIN32
unsigned long get_tick_count()
{
	return GetTickCount();
}
#else
unsigned long get_tick_count()
{
	unsigned long   ret;
	struct  timeval time_val;

	gettimeofday(&time_val, NULL);
	ret = time_val.tv_sec * 1000 + time_val.tv_usec / 1000;

	return ret;
}
#endif // CM_WIN32
