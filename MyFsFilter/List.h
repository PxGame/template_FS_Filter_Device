# ifndef _LIST_H_
# define _LIST_H_
# include <ntifs.h>
# include <fcb.h>

typedef struct {
	LIST_ENTRY list_entry;
	FCB *fcb;
}LT_NODE, *PLT_NODE;

inline BOOLEAN InitedList()
{
	return s_list_inited;
}

inline void LockList()
{
	ASSERT(s_list_inited);
	KeAcquireSpinLock(&s_list_lock, &s_list_lock_irql);
}

inline void UnLockList()
{
	ASSERT(s_list_inited);
	KeReleaseSpinLock(&s_list_lock, s_list_lock_irql);
}

inline void InitList()
{
	InitializeListHead(&s_list);
	KeInitializeSpinLock(&s_list_lock);
	s_list_inited = TRUE;
}


# endif