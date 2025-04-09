#include "platform/platform.h"
#if _PLATFORM_HAS_SCHEDULING

#include <stdatomic.h>
#include <stdbool.h>
#include <sys/proc.h>
#include <threads.h>

#include "common/thread.h"

__attribute__((noreturn)) __attribute__((force_align_arg_pointer)) static void _ThrdEntry(_Thread_t* thread, thrd_start_t func,
    void* arg)
{
    while (!atomic_load(&thread->running))
    {
        yield();
    }

    int res = func(arg);
    thrd_exit(res);
}

int thrd_create(thrd_t* thr, thrd_start_t func, void* arg)
{
    _Thread_t* thread = _ThreadNew(); // Dereference base reference
    if (thread == NULL)
    {
        return thrd_error;
    }

    thread->id = split(_ThrdEntry, 3, thread, func, arg);
    if (thread->id == ERR)
    {
        _ThreadFree(thread);
        return thrd_error;
    }

    atomic_store(&thread->running, true);

    thr->thread = thread;
    return thrd_success;
}

int thrd_equal(thrd_t lhs, thrd_t rhs)
{
    return (lhs.thread == rhs.thread);
}

thrd_t thrd_current(void)
{
    _Thread_t* thread = _ThreadById(gettid());
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
        sleep(nanoseconds);
        nsec_t end = uptime();

        nsec_t timeTaken = end - start;
        remaining->tv_sec = timeTaken / SEC;
        remaining->tv_nsec = timeTaken % SEC;
    }
    else
    {
        sleep(nanoseconds);
    }

    return 0;
}

void thrd_yield(void)
{
    yield();
}

_NORETURN void thrd_exit(int res)
{
    _Thread_t* thread = _ThreadById(gettid());
    thread->result = res;
    atomic_store(&thread->running, false);
    _ThreadUnref(thread);
    _ThreadUnref(thread); // Dereference base reference
    thread_exit();
}

int thrd_detach(thrd_t thr)
{
    // TODO: Implement this
    return thrd_error;
}

int thrd_join(thrd_t thr, int* res)
{
    _Thread_t* thread = _ThreadRef(thr.thread);

    // TODO: Implement kernel side blocking for this
    while (atomic_load(&thread->running))
    {
        sleep(SEC / 1000);
    }

    if (res != NULL)
    {
        *res = thread->result;
    }
    _ThreadUnref(thread);
    return thrd_success;
}

// TODO: High priority, implement a proper user space mutex, futex?
int mtx_init(mtx_t* mutex, int type)
{
    if (type != mtx_plain)
    {
        return thrd_error;
    }

    atomic_init(&mutex->nextTicket, 0);
    atomic_init(&mutex->nowServing, 0);

    return thrd_success;
}

int mtx_lock(mtx_t* mutex)
{
    int ticket = atomic_fetch_add(&mutex->nextTicket, 1);
    while (atomic_load(&mutex->nowServing) != ticket)
    {
        asm volatile("pause");
    }

    return thrd_success;
}

int mtx_unlock(mtx_t* mutex)
{
    atomic_fetch_add(&mutex->nowServing, 1);

    return thrd_success;
}

#endif
