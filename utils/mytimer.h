#if !defined(_P2P_TRANSPORT_TIMER_H_INCLUDED_)
#define _P2P_TRANSPORT_TIMER_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*P2PTimerProc)(void* pPtr, unsigned long uID);

typedef struct time_node
{
	unsigned long ulNextTime;
	int nInterval;
	void *pPtr;
	int nRemainCount;
	P2PTimerProc OnTimer;
	unsigned long ulID;
}time_node;

unsigned long get_tick_count();
void InitTimerQue();
void UnInitTimerQue();
long ProcessTimer();
unsigned long RegisterTimer(int milisecond, P2PTimerProc callback, void* pParam, int nRepeatTime);
void CancelTimer(unsigned long ulTimerID);

#ifdef __cplusplus
}
#endif

#endif //!define _P2P_TRANSPORT_TIMER_H_INCLUDED_

