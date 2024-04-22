#include "lock.h"

#include "trap.h"

void lock_init(Lock* lock)
{
    atomic_init(&lock->nextTicket, 0);
    atomic_init(&lock->nowServing, 0);
}

void lock_acquire(Lock* lock)
{
    interrupts_disable();

    //Overflow does not matter
    uint32_t ticket = atomic_fetch_add(&lock->nextTicket, 1);
    while (atomic_load(&lock->nowServing) != ticket)
    {
        asm volatile("pause");
    }
}

void lock_release(Lock* lock)
{
    atomic_fetch_add(&lock->nowServing, 1);

    interrupts_enable();
}