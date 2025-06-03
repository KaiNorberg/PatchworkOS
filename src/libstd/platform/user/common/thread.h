#pragma once

#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/list.h>
#include <sys/proc.h>
#include <threads.h>

#define _MTX_SPIN_COUNT 100

#define _THREAD_ENTRY_ATTRIBUTES __attribute__((noreturn)) __attribute__((force_align_arg_pointer))

typedef struct _Thread _Thread_t;

typedef void (*_ThreadEntry_t)(_Thread_t*);

#define _THREAD_ATTACHED 1
#define _THREAD_DETACHED 2
#define _THREAD_JOINING 3
#define _THREAD_EXITED 4

typedef struct _Thread
{
    list_entry_t entry;
    atomic_uint64_t state;
    tid_t id;
    int64_t result;
    errno_t err;
    void* private;
} _Thread_t;

void _ThreadingInit(void);

_Thread_t* _ThreadNew(_ThreadEntry_t entry, void* private);

void _ThreadFree(_Thread_t* thread);

_Thread_t* _ThreadGet(tid_t id);