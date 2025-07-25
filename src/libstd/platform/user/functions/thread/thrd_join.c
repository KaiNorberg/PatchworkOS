#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "platform/user/common/syscalls.h"
#include "platform/user/common/thread.h"

int thrd_join(thrd_t thr, int* res)
{
    _thread_t* thread = _thread_get(thr.id);
    if (thread == NULL)
    {
        return thrd_error;
    }

    uint64_t expected = _THREAD_ATTACHED;
    if (!atomic_compare_exchange_strong(&thread->state, &expected, _THREAD_JOINING))
    {
        if (expected == _THREAD_DETACHED)
        {
            return thrd_error;
        }
    }

    uint64_t state;
    while ((state = atomic_load(&thread->state)) != _THREAD_EXITED)
    {
        futex(&thread->state, state, FUTEX_WAIT, CLOCKS_NEVER);
    }

    if (res != NULL)
    {
        *res = thread->result;
    }

    _thread_free(thread);
    return thrd_success;
}
