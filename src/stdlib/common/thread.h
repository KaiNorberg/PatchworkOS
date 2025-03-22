#pragma once
#include "../platform/platform.h"
#if _PLATFORM_HAS_SCHEDULING

#include <stdatomic.h>
#include <stdbool.h>
#include <sys/proc.h>
#include <threads.h>

#define _MAX_THRD 32

typedef struct
{
    atomic_long ref;
    atomic_bool running;
    uint8_t index;
    tid_t id;
    uint8_t result;
    int err;
} _Thread_t;

void _ThreadingInit(void);

_Thread_t* _ThreadReserve(void);

void _ThreadFree(_Thread_t* thread);

uint64_t _ThrdIndexById(tid_t id);

_Thread_t* _ThreadById(tid_t id);

_Thread_t* _ThreadByIndex(uint64_t index);

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
