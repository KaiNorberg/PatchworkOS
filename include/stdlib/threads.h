#ifndef _THREADS_H
#define _THREADS_H 1

#include <sys/atomint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/config.h"
#include "_AUX/pid_t.h"
#include "_AUX/tid_t.h"
#include "_AUX/timespec.h"

#include <stdatomic.h>

enum
{
    thrd_success = 0,
    thrd_nomem = 1,
    thrd_timedout = 2,
    thrd_busy = 3,
    thrd_error = 4
};

typedef struct _Thread _Thread_t;
typedef struct
{
    _Thread_t* thread;
} thrd_t;

enum
{
    mtx_plain = 0,
    mtx_recursive = (1 << 0),
    mtx_timed = (1 << 1),
};

// TODO: High priority, implement a proper user space mutex, futex?
typedef struct
{
    atomic_uint64 state;
} mtx_t;

typedef int (*thrd_start_t)(void*);

int thrd_create(thrd_t* thr, thrd_start_t func, void* arg);

int thrd_equal(thrd_t lhs, thrd_t rhs);

thrd_t thrd_current(void);

int thrd_sleep(const struct timespec* duration, struct timespec* remaining);

void thrd_yield(void);

_NORETURN void thrd_exit(int res);

int thrd_detach(thrd_t thr);

int thrd_join(thrd_t thr, int* res);

int mtx_init(mtx_t* mutex, int type);

int mtx_lock(mtx_t* mutex);

int mtx_unlock(mtx_t* mutex);

#if defined(__cplusplus)
}
#endif

#endif
