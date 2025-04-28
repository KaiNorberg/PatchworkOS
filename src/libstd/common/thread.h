#pragma once
#include "../platform/platform.h"
#if _PLATFORM_HAS_SYSCALLS

#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/list.h>
#include <sys/proc.h>
#include <threads.h>

#define _MTX_SPIN_COUNT 100

typedef struct _Thread
{
    list_entry_t entry;
    atomic_long ref;
    atomic_uint64 running;
    tid_t id;
    uint8_t result;
    errno_t err;
    thrd_start_t func;
    void* arg;
} _Thread_t;

void _ThreadingInit(void);

_Thread_t* _ThreadNew(thrd_start_t func, void* arg);

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
