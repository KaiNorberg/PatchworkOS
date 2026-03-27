#include <libstd/sync.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <threads.h>

#include "user/common/threading.h"

int mtx_unlock(mtx_t* mutex)
{
    thrd_t self = thrd_current();
    if (mutex->owner != self)
    {
        return thrd_error;
    }

    mutex->depth--;
    if (mutex->depth > 0)
    {
        return thrd_success;
    }
    mutex->owner = -1;

    if (atomic_exchange(&(mutex->state), _MTX_UNLOCKED) == _MTX_CONTESTED)
    {
        sync_ctl(&(mutex->state), 1, SYNC_WAKE, CLOCKS_NEVER, NULL);
    }
    return thrd_success;
}
