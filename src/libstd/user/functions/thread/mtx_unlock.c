#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "user/common/syscalls.h"
#include "user/common/thread.h"

int mtx_unlock(mtx_t* mutex)
{
    tid_t self = gettid();
    if (mutex->owner != self)
    {
        return thrd_error;
    }

    mutex->depth--;
    if (mutex->depth > 0)
    {
        return thrd_success;
    }
    mutex->owner = ERR;

    if (atomic_exchange(&(mutex->state), _MTX_UNLOCKED) == _MTX_CONTESTED)
    {
        futex(&(mutex->state), 1, FUTEX_WAKE, CLOCKS_NEVER);
    }
    return thrd_success;
}
