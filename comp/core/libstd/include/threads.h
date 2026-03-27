#ifndef _THREADS_H
#define _THREADS_H 1

#include <libstd/syscall.h>
#include <stdatomic.h>

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @brief Thread management.
 * @ingroup libstd
 * @defgroup libstd_threads Threads
 *
 * @todo Implement user space `cnd_t` and `tss_t`.
 *
 * @{
 */

#include "_libstd/config.h"
#include "_libstd/timespec.h"

#if __STDC_NO_THREADS__ == 1
#error __STDC_NO_THREADS__ defined but <threads.h> included. Something is wrong about your setup.
#endif

/**
 * @brief Thread identifier type.
 * @typedef thrd_t
 *
 * The `thrd_t` type is used to identify a thread within a process, not system wide, both from user-space and
 * kernel-space.
 */
typedef __UINT64_TYPE__ thrd_t;

#ifndef _KERNEL_

typedef struct
{
    char todo;
} cnd_t;

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
    thrd_t owner;
    uint64_t depth;
} mtx_t;

#define _THRD_STACK 8192

#if __STDC_VERSION__ >= 201112L
#define thread_local _Thread_local
#endif

#define ONCE_FLAG_INIT 0

#define TSS_DTOR_ITERATIONS 4

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

int thrd_create(thrd_t* thr, thrd_start_t func, void* arg);

/**
 * @brief System call to retrieve the current thread identifier.
 *
 * @return The running threads identifier.
 */
static inline thrd_t thrd_current(void)
{
    thrd_t tid;
    syscall0(SYS_THRD_CURRENT, &tid);
    return tid;
}

int thrd_detach(thrd_t thr);

int thrd_equal(thrd_t thr0, thrd_t thr1);

_NORETURN void thrd_exit(int res);

int thrd_join(thrd_t thr, int* res);

int thrd_sleep(const struct timespec* duration, struct timespec* remaining);

void thrd_yield(void);

void call_once(once_flag* flag, void (*func)(void));

int cnd_broadcast(cnd_t* cond);

void cnd_destroy(cnd_t* cond);

int cnd_init(cnd_t* cond);

int cnd_signal(cnd_t* cond);

int cnd_timedwait(cnd_t* _RESTRICT cond, mtx_t* _RESTRICT mtx, const struct timespec* _RESTRICT ts);

int cnd_wait(cnd_t* cond, mtx_t* mtx);

void mtx_destroy(mtx_t* mtx);

int mtx_init(mtx_t* mtx, int type);

int mtx_lock(mtx_t* mtx);

int mtx_timedlock(mtx_t* _RESTRICT mtx, const struct timespec* _RESTRICT ts);

int mtx_trylock(mtx_t* mtx);

int mtx_unlock(mtx_t* mtx);

int tss_create(tss_t* key, tss_dtor_t dtor);

void tss_delete(tss_t key);

void* tss_get(tss_t key);

int tss_set(tss_t key, void* val);

#endif

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
