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
    _thread_t* thread = _thread_get(gettid());
    if (thread == NULL)
    {
        abort();
    }

    thread->result = res;

    uint64_t state = atomic_exchange(&thread->state, _THREAD_EXITED);
    if (state == _THREAD_DETACHED)
    {
        _thread_free(thread);
    }
    else
    {
        futex(&thread->state, FUTEX_ALL, FUTEX_WAKE, CLOCKS_NEVER);
    }

    _syscall_thread_exit();
}
