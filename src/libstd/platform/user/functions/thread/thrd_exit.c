#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/proc.h>
#include <threads.h>

#include "platform/user/common/syscalls.h"
#include "platform/user/common/thread.h"

void thrd_exit(int res)
{
    _Thread_t* thread = _ThreadGet(_SyscallGetTid());
    if (thread == NULL)
    {
        printf("abort %d\n", _SyscallGetTid());
        abort();
    }

    thread->result = res;

    uint64_t state = atomic_exchange(&thread->state, _THREAD_EXITED);
    if (state == _THREAD_DETACHED)
    {
        _ThreadFree(thread);
    }
    else
    {
        futex(&thread->state, FUTEX_ALL, FUTEX_WAKE, CLOCKS_NEVER);
    }

    _SyscallThreadExit();
}
