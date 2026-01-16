#pragma once

#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/proc.h>
#include <threads.h>

/**
 * @brief Threading.
 * @defgroup libstd_common_user_threading Threading
 * @ingroup libstd_common_user
 *
 * @todo Write threading documentation.
 *
 * @todo Implement Thread Local Storage (TLS).
 *
 * @{
 */

#define _MTX_SPIN_COUNT 100

#define _THREADS_MAX 2048

typedef struct _thread _thread_t;

typedef void (*_thread_entry_t)(_thread_t*);

#define _THREAD_ATTACHED 1
#define _THREAD_DETACHED 2
#define _THREAD_JOINING 3
#define _THREAD_EXITED 4

typedef struct _thread
{
    _thread_t* self;
    atomic_uint64_t state;
    tid_t id;
    int result;
    errno_t err;
    thrd_start_t func;
    void* arg;
} _thread_t;

void _threading_init(void);

_thread_t* _thread_new(thrd_start_t func, void* arg);

void _thread_free(_thread_t* thread);

_thread_t* _thread_get(tid_t id);

#define _THREAD_SELF ((_thread_t __seg_fs*)0)

/** @} */