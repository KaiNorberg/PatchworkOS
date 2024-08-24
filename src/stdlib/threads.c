#ifndef __EMBED__

#include <stdatomic.h>
#include <stdbool.h>
#include <sys/proc.h>

#include "internal/thrd.h"

static void _ThrdEntry(void)
{
    sleep(SEC / 1000); // HIGH PRIORITY TODO: Implement user space mutex to avoid this stuff
    thrd_block_t* block = _ThrdBlockById(gettid());
    while (!atomic_load(&block->running))
    {
        yield();
    }

    thrd_start_t func = block->func;
    void* arg = block->arg;
    _ThrdBlockUnref(block);

    int res = func(arg);
    thrd_exit(res);
}

int thrd_create(thrd_t* thr, thrd_start_t func, void* arg)
{
    thrd_block_t* block = _ThrdBlockReserve();
    if (block == NULL)
    {
        return thrd_error;
    }

    tid_t id = split(_ThrdEntry);
    if (block->id == ERR)
    {
        _ThrdBlockFree(block);
        return thrd_error;
    }

    _ThrdBlockInit(block, func, arg, id);
    atomic_store(&block->running, true);

    thr->index = block->index;
    return thrd_success;
}

int thread_equal(thrd_t lhs, thrd_t rhs)
{
    return (lhs.index == rhs.index);
}

thrd_t thrd_current(void)
{
    thrd_block_t* block = _ThrdBlockById(gettid());
    thrd_t thr = {.index = block->index};

    _ThrdBlockUnref(block);
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
    thrd_block_t* block = _ThrdBlockById(gettid());
    block->result = res;
    atomic_store(&block->running, false);
    _ThrdBlockUnref(block);
    //_ThrdBlockUnref(block); // Dereference base reference
    thread_exit();
}

int thrd_detach(thrd_t thr)
{
    // TODO: Implement this
    return thrd_error;
}

int thrd_join(thrd_t thr, int* res)
{
    thrd_block_t* block = _ThrdBlockByIndex(thr.index);
    if (block == NULL)
    {
        return thrd_error;
    }

    // TODO: Implement kernel side blocking for this
    while (atomic_load(&block->running))
    {
        sleep(SEC / 1000);
    }

    if (res != NULL)
    {
        *res = block->result;
    }
    _ThrdBlockUnref(block);
    return thrd_success;
}

#endif
