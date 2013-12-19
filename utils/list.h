#if !defined(_UDP_TRANSPORT_LIST_H_INCLUDED_)
#define _UDP_TRANSPORT_LIST_H_INCLUDED_

#include "p2pcommon.h"

typedef void* ElementType;
struct Node;
typedef struct Node *PtrToNode;

typedef PtrToNode Position;

struct Node
{
	ElementType Element;
	Position Next;
};

typedef struct ListHeader
{
	ElementType Element;
	Position Next;
	THREAD_MUTEX_T lock;
}*List;

#ifdef __cplusplus
extern "C" {
#endif

List MakeEmpty(List L);
int IsEmpty( List L );
int IsLast(Position P, List L);
Position Find(ElementType X, List L);
void Delete( ElementType X, List L );
Position FindPrevious(ElementType X, List L);
void Insert( ElementType X, List L, Position P);
void DeleteList(List L);
Position Header(List L);
Position First(List L);
Position Advance(Position P);
ElementType Retrieve(Position P);
void LockList(List L);
void UnLockList(List L);

#ifdef __cplusplus
}
#endif

#endif //!define _UDP_TRANSPORT_LIST_H_INCLUDED_
