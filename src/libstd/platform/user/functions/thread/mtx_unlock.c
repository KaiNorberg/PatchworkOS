#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "platform/user/common/syscalls.h"
#include "platform/user/common/thread.h"

int mtx_unlock(mtx_t* mutex)
{
    if (atomic_exchange(&(mutex->state), FUTEX_UNLOCKED) == FUTEX_CONTESTED)
    {
        futex(&(mutex->state), 1, FUTEX_WAKE, CLOCKS_NEVER);
    }
    return thrd_success;
}
