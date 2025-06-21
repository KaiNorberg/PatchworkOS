#pragma once

#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/list.h>
#include <sys/proc.h>
#include <threads.h>

#define _MTX_SPIN_COUNT 100

#define _THREAD_ENTRY_ATTRIBUTES __attribute__((noreturn)) __attribute__((force_align_arg_pointer))

typedef struct _thread _thread_t;

typedef void (*_thread_entry_t)(_thread_t*);

#define _THREAD_ATTACHED 1
#define _THREAD_DETACHED 2
#define _THREAD_JOINING 3
#define _THREAD_EXITED 4

typedef struct _thread
{
    list_entry_t entry;
    atomic_uint64_t state;
    tid_t id;
    int64_t result;
    errno_t err;
    void* private;
} _thread_t;

void _threading_init(void);

_thread_t* _thread_new(_thread_entry_t entry, void* private);

void _thread_free(_thread_t* thread);

_thread_t* _thread_get(tid_t id);