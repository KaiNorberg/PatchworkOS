#ifndef _THREADS_H
#define _THREADS_H 1

#include <stdatomic.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libstd/config.h"
#include "_libstd/pid_t.h"
#include "_libstd/tid_t.h"
#include "_libstd/timespec.h"

#include <stdatomic.h>

#if __STDC_NO_THREADS__ == 1
#error __STDC_NO_THREADS__ defined but <threads.h> included. Something is wrong about your setup.
#endif

#if __STDC_VERSION__ >= 201112L
#define thread_local _Thread_local
#endif

#define ONCE_FLAG_INIT 0

#define TSS_DTOR_ITERATIONS 4

/// @todo Implement user space `cnd_t` and `tss_t`.

typedef struct
{
    char todo;
} cnd_t;

typedef struct
{
    tid_t id;
} thrd_t;

typedef struct
{
    char todo;
} tss_t;

#define _MTX_UNLOCKED 0
#define _MTX_LOCKED 1
#define _MTX_CONTESTED 2

typedef struct
{
    atomic_uint64_t state;
    tid_t owner;
    uint64_t depth;
} mtx_t;

typedef void (*tss_dtor_t)(void*);

typedef int (*thrd_start_t)(void*);

typedef int once_flag;

enum
{
    mtx_plain,
    mtx_recursive,
    mtx_timed
};

enum
{
    thrd_timedout,
    thrd_success,
    thrd_busy,
    thrd_error,
    thrd_nomem
};

_PUBLIC void call_once(once_flag* flag, void (*func)(void));

_PUBLIC int cnd_broadcast(cnd_t* cond);

_PUBLIC void cnd_destroy(cnd_t* cond);

_PUBLIC int cnd_init(cnd_t* cond);

_PUBLIC int cnd_signal(cnd_t* cond);

_PUBLIC int cnd_timedwait(cnd_t* _RESTRICT cond, mtx_t* _RESTRICT mtx, const struct timespec* _RESTRICT ts);

int cnd_wait(cnd_t* cond, mtx_t* mtx);

_PUBLIC void mtx_destroy(mtx_t* mtx);

_PUBLIC int mtx_init(mtx_t* mtx, int type);

_PUBLIC int mtx_lock(mtx_t* mtx);

_PUBLIC int mtx_timedlock(mtx_t* _RESTRICT mtx, const struct timespec* _RESTRICT ts);

_PUBLIC int mtx_trylock(mtx_t* mtx);

_PUBLIC int mtx_unlock(mtx_t* mtx);

_PUBLIC int thrd_create(thrd_t* thr, thrd_start_t func, void* arg);

_PUBLIC thrd_t thrd_current(void);

_PUBLIC int thrd_detach(thrd_t thr);

_PUBLIC int thrd_equal(thrd_t thr0, thrd_t thr1);

_PUBLIC _NORETURN void thrd_exit(int res);

_PUBLIC int thrd_join(thrd_t thr, int* res);

_PUBLIC int thrd_sleep(const struct timespec* duration, struct timespec* remaining);

_PUBLIC void thrd_yield(void);

_PUBLIC int tss_create(tss_t* key, tss_dtor_t dtor);

_PUBLIC void tss_delete(tss_t key);

_PUBLIC void* tss_get(tss_t key);

_PUBLIC int tss_set(tss_t key, void* val);

#if defined(__cplusplus)
}
#endif

#endif
