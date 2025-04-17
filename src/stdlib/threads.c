#include "platform/platform.h"
#if _PLATFORM_HAS_SYSCALLS

#include <stdatomic.h>
#include <stdbool.h>
#include <sys/proc.h>
#include <threads.h>

#include "common/thread.h"

#include <stdio.h>

__attribute__((noreturn)) __attribute__((force_align_arg_pointer)) static void _ThrdEntry(_Thread_t* thread)
{
    while (!atomic_load(&thread->running))
    {
        _SyscallYield();
    }

    int res = thread->func(thread->arg);
    thrd_exit(res);
}

int thrd_create(thrd_t* thr, thrd_start_t func, void* arg)
{
    _Thread_t* thread = _ThreadNew(func, arg); // Dereference base reference
    if (thread == NULL)
    {
        return thrd_error;
    }

    thread->id = _SyscallThreadCreate(_ThrdEntry, thread);
    if (thread->id == ERR)
    {
        _ThreadFree(thread);
        return thrd_error;
    }

    thr->thread = thread;
    atomic_store(&thread->running, true);
    return thrd_success;
}

int thrd_equal(thrd_t lhs, thrd_t rhs)
{
    return (lhs.thread == rhs.thread);
}

thrd_t thrd_current(void)
{
    _Thread_t* thread = _ThreadById(_SyscallThreadId());
    thrd_t thr = {.thread = thread};

    _ThreadUnref(thread);
    return thr;
}

int thrd_sleep(const struct timespec* duration, struct timespec* remaining)
{
    uint64_t nanoseconds = (uint64_t)duration->tv_sec * SEC + (uint64_t)duration->tv_nsec;

    if (remaining != NULL)
    {
        nsec_t start = uptime();
        _SyscallSleep(nanoseconds);
        nsec_t end = uptime();

        nsec_t timeTaken = end - start;
        remaining->tv_sec = timeTaken / SEC;
        remaining->tv_nsec = timeTaken % SEC;
    }
    else
    {
        _SyscallSleep(nanoseconds);
    }

    return 0;
}

void thrd_yield(void)
{
    _SyscallYield();
}

_NORETURN void thrd_exit(int res)
{
    _Thread_t* thread = _ThreadById(_SyscallThreadId());
    thread->result = res;
    atomic_store(&thread->running, false);
    futex(&thread->running, FUTEX_ALL, FUTEX_WAKE, NEVER);
    _ThreadUnref(thread);

    _ThreadUnref(thread); // Dereference base reference
    _SyscallThreadExit();
}

int thrd_detach(thrd_t thr)
{
    // TODO: Implement this
    return thrd_error;
}

int thrd_join(thrd_t thr, int* res)
{
    _Thread_t* thread = _ThreadRef(thr.thread);
    futex(&thread->running, true, FUTEX_WAIT, NEVER);
    if (res != NULL)
    {
        *res = thread->result;
    }
    _ThreadUnref(thread);
    return thrd_success;
}

int mtx_init(mtx_t* mutex, int type)
{
    if (type != mtx_plain)
    {
        return thrd_error;
    }

    atomic_init(&mutex->state, FUTEX_UNLOCKED);

    return thrd_success;
}

void mtx_destory(mtx_t* mutex)
{
    // Do nothing
}

int mtx_lock(mtx_t* mutex)
{
    uint64_t expected = FUTEX_UNLOCKED;
    if (atomic_compare_exchange_strong(&(mutex->state), &expected, FUTEX_LOCKED))
    {
        return thrd_success;
    }

    for (int i = 0; i < _MTX_SPIN_COUNT; ++i)
    {
        if (atomic_load(&(mutex->state)) == FUTEX_UNLOCKED)
        {
            expected = FUTEX_UNLOCKED;
            if (atomic_compare_exchange_strong(&(mutex->state), &expected, FUTEX_LOCKED))
            {
                return thrd_success;
            }
        }
        asm volatile("pause");
    }

    do
    {
        expected = FUTEX_UNLOCKED;
        if (atomic_compare_exchange_strong(&(mutex->state), &expected, FUTEX_LOCKED))
        {
            return thrd_success;
        }

        uint64_t current = atomic_load(&(mutex->state));
        if (current != FUTEX_CONTESTED)
        {
            expected = current;
            atomic_compare_exchange_strong(&(mutex->state), &expected, FUTEX_CONTESTED);
        }
        futex(&(mutex->state), FUTEX_CONTESTED, FUTEX_WAIT, NEVER);
    } while (1);

    return thrd_success;
}

int mtx_unlock(mtx_t* mutex)
{
    if (atomic_exchange(&(mutex->state), FUTEX_UNLOCKED) == FUTEX_CONTESTED)
    {
        futex(&(mutex->state), 1, FUTEX_WAKE, NEVER);
    }
    return thrd_success;
}

#endif
