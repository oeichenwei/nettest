#include "p2pcommon.h"
#include "list.h"
#include <stdlib.h>

List MakeEmpty(List L)
{
	if(L != NULL)
		DeleteList(L);

	L = malloc(sizeof(struct ListHeader));
	if(L == NULL)
	{
		logMessage(logInfo, "Out of memory!");
		return NULL;
	}
	else
	{
		L->Element = NULL;
		L->Next = NULL;
		InitLock(&L->lock);
	}
	return L;
}

int IsEmpty(List L)
{
	if(L == NULL)
		return 1;

	return L->Next == NULL;
}

int IsLast(Position P, List L)
{
	if(P == NULL)
		return 1;

	return P->Next == NULL;
}

Position Find(ElementType X, List L)
{
	Position P;

	if(L == NULL)
		return NULL;

	P = L->Next;
	while( P != NULL && P->Element != X)
		P = P->Next;
	
	return P;
}

void Delete(ElementType X, List L)
{
	Position P, TmpCell;

	P = FindPrevious(X, L);

	if(!IsLast(P, L))
	{
		TmpCell = P->Next;
		P->Next = TmpCell->Next;
		free(TmpCell);
	}
}

Position FindPrevious( ElementType X, List L)
{
	Position P;

	if(L == NULL)
		return NULL;

	P = (Position)L;
	while( P->Next != NULL && P->Next->Element != X)
		P = P->Next;

	return P;
}

void Insert(ElementType X, List L, Position P)
{
	Position TmpCell;

	TmpCell = malloc(sizeof(struct Node));
	if(TmpCell == NULL)
	{
		assert(false);
		logMessage(logInfo, "Out of space!!!");
		return;
	}

	TmpCell->Element = X;
	TmpCell->Next = P->Next;
	P->Next = TmpCell;
}

void DeleteList(List L)
{
	Position P, Tmp;

	if(L == NULL)
		return;

	Lock(&L->lock);
	P = L->Next;
	L->Next = NULL;
	while(P != NULL)
	{
		Tmp = P->Next;
		free(P);
		P = Tmp;
	}
	UnLock(&L->lock);
	UnInitLock(&L->lock);
	free(L);
}

Position Header(List L)
{
	return (Position)L;
}

Position First(List L)
{
	if(L == NULL)
		return NULL;
	return L->Next;
}

Position Advance(Position P)
{
	if(P == NULL)
		return NULL;

	return P->Next;
}

ElementType Retrieve(Position P)
{
	if(P == NULL)
		return NULL;

	return P->Element;
}

void LockList(List L)
{
	if(L == NULL)
		return;
	Lock(&L->lock);
}

void UnLockList(List L)
{
	if(L == NULL)
		return;
	UnLock(&L->lock);
}
