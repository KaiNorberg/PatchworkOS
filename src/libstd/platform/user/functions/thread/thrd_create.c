#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "platform/user/common/syscalls.h"
#include "platform/user/common/thread.h"

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
