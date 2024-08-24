#ifndef _THREADS_H
#define _THREADS_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/config.h"
#include "_AUX/pid_t.h"
#include "_AUX/tid_t.h"
#include "_AUX/timespec.h"

enum
{
    thrd_success = 0,
    thrd_nomem = 1,
    thrd_timedout = 2,
    thrd_busy = 3,
    thrd_error = 4
};

typedef struct
{
    uint64_t index;
} thrd_t;

typedef int (*thrd_start_t)(void*);

int thrd_create(thrd_t* thr, thrd_start_t func, void* arg);

int thread_equal(thrd_t lhs, thrd_t rhs);

thrd_t thrd_current(void);

int thrd_sleep(const struct timespec* duration, struct timespec* remaining);

void thrd_yield(void);

_NORETURN void thrd_exit(int res);

int thrd_detach(thrd_t thr);

int thrd_join(thrd_t thr, int* res);

#if defined(__cplusplus)
}
#endif

#endif
