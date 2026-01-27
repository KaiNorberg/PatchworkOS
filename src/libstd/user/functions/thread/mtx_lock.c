#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/proc.h>
#include <threads.h>

#include "user/common/syscalls.h"
#include "user/common/threading.h"

int mtx_lock(mtx_t* mutex)
{
    tid_t self = gettid();
    if (mutex->owner == self)
    {
        mutex->depth++;
        return thrd_success;
    }

    for (uint64_t i = 0; i < _MTX_SPIN_COUNT; i++)
    {
        uint64_t expected = _MTX_UNLOCKED;
        if (atomic_compare_exchange_strong(&(mutex->state), &expected, _MTX_LOCKED))
        {
            mutex->owner = self;
            mutex->depth = 1;
            return thrd_success;
        }
        ASM("pause");
    }

    do
    {
        uint64_t expected = _MTX_UNLOCKED;
        if (atomic_compare_exchange_strong(&(mutex->state), &expected, _MTX_LOCKED))
        {
            mutex->owner = self;
            mutex->depth = 1;
            return thrd_success;
        }

        if (expected == _MTX_LOCKED)
        {
            atomic_compare_exchange_strong(&(mutex->state), &expected, _MTX_CONTESTED);
        }
        futex(&(mutex->state), _MTX_CONTESTED, FUTEX_WAIT, CLOCKS_NEVER, NULL);
    } while (1);
}
