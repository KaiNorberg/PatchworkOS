#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "platform/user/common/syscalls.h"
#include "platform/user/common/thread.h"

int mtx_lock(mtx_t* mutex)
{
    tid_t self = gettid();
    if (mutex->owner == self)
    {
        mutex->depth++;
        return thrd_success;
    }

    uint64_t expected = FUTEX_UNLOCKED;
    if (atomic_compare_exchange_strong(&(mutex->state), &expected, FUTEX_LOCKED))
    {
        mutex->owner = self;
        mutex->depth = 1;
        return thrd_success;
    }

    for (int i = 0; i < _MTX_SPIN_COUNT; ++i)
    {
        if (atomic_load(&(mutex->state)) == FUTEX_UNLOCKED)
        {
            expected = FUTEX_UNLOCKED;
            if (atomic_compare_exchange_strong(&(mutex->state), &expected, FUTEX_LOCKED))
            {
                mutex->owner = self;
                mutex->depth = 1;
                return thrd_success;
            }
        }
        asm volatile("pause");
    }

    do
    {
        expected = FUTEX_UNLOCKED;
        if (atomic_compare_exchange_strong(&(mutex->state), &expected, FUTEX_LOCKED))
        {
            mutex->owner = self;
            mutex->depth = 1;
            return thrd_success;
        }

        uint64_t current = atomic_load(&(mutex->state));
        if (current != FUTEX_CONTESTED)
        {
            expected = current;
            atomic_compare_exchange_strong(&(mutex->state), &expected, FUTEX_CONTESTED);
        }
        futex(&(mutex->state), FUTEX_CONTESTED, FUTEX_WAIT, CLOCKS_NEVER);
    } while (1);
}
