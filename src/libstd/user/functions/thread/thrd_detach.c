#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "user/common/syscalls.h"
#include "user/common/thread.h"

int thrd_detach(thrd_t thr)
{
    _thread_t* thread = _thread_get(thr.id);
    if (thread == NULL)
    {
        return thrd_error;
    }

    uint64_t expected = _THREAD_ATTACHED;
    if (!atomic_compare_exchange_strong(&thread->state, &expected, _THREAD_DETACHED))
    {
        return thrd_error;
    }

    return thrd_success;
}
