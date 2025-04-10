#pragma once
#include "../platform/platform.h"
#if _PLATFORM_HAS_SCHEDULING

#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/list.h>
#include <sys/proc.h>
#include <threads.h>

typedef struct _Thread
{
    list_entry_t entry;
    atomic_long ref;
    atomic_bool running;
    tid_t id;
    uint8_t result;
    errno_t err;
} _Thread_t;

void _ThreadingInit(void);

_Thread_t* _ThreadNew(void);

void _ThreadFree(_Thread_t* thread);

_Thread_t* _ThreadById(tid_t id);

static inline _Thread_t* _ThreadRef(_Thread_t* thread)
{
    atomic_fetch_add(&thread->ref, 1);
    return thread;
}

static inline void _ThreadUnref(_Thread_t* thread)
{
    if (atomic_fetch_sub(&thread->ref, 1) <= 1)
    {
        _ThreadFree(thread);
    }
}

#endif
