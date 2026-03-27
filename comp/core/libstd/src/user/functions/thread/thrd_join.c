#include <libstd/sync.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <threads.h>
#include <time.h>

#include "user/common/threading.h"

int thrd_join(thrd_t thr, int* res)
{
    _thread_t* thread = _thread_get(thr);
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

    while (true)
    {
        uint64_t state = atomic_load(&thread->state);
        if (state == _THREAD_EXITED)
        {
            break;
        }

        sync_ctl(&thread->state, state, SYNC_WAIT, CLOCKS_NEVER, NULL);
    }

    if (res != NULL)
    {
        *res = thread->result;
    }

    _thread_free(thread);
    return thrd_success;
}
