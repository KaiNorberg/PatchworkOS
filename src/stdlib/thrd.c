#include "platform/platform.h"
#if _PLATFORM_HAS_SCHEDULING

#include <stdatomic.h>
#include <stdbool.h>
#include <sys/proc.h>

#include "common/thread.h"

__attribute__((noreturn)) __attribute__((force_align_arg_pointer)) static void _ThrdEntry(_Thread_t* thread, thrd_start_t func, void* arg)
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
    _Thread_t* thread = _ThreadReserve(); // Base reference
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

    thr->index = thread->index;
    return thrd_success;
}

int thread_equal(thrd_t lhs, thrd_t rhs)
{
    return (lhs.index == rhs.index);
}

thrd_t thrd_current(void)
{
    _Thread_t* thread = _ThreadById(gettid());
    thrd_t thr = {.index = thread->index};

    _ThreadUnref(thread);
    return thr;
}

int thrd_sleep(const struct timespec* duration, struct timespec* remaining)
{
    // TODO: Sleep is currently uninterruptible so "remaining" is ignored.
    uint64_t nanoseconds = (uint64_t)duration->tv_sec * 1000000000ULL + (uint64_t)duration->tv_nsec;
    sleep(nanoseconds);
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
    _Thread_t* thread = _ThreadByIndex(thr.index);
    if (thread == NULL)
    {
        return thrd_error;
    }

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

#endif
