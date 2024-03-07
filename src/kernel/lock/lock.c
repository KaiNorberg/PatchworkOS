#include "lock.h"

#include "interrupts/interrupts.h"

Lock lock_new()
{
    return (Lock)
    {
        .servingTicket = 0,
        .nextTicket = 0
    };
}

void lock_acquire(Lock* lock)
{
    interrupts_disable();

    //Overflow does not matter
    int32_t ticket = atomic_fetch_add_explicit(&lock->nextTicket, 1, memory_order_seq_cst);
    while (atomic_load(&lock->servingTicket) != ticket)
    {
        asm volatile("pause");
    }
}

void lock_release(Lock* lock)
{
    atomic_fetch_add_explicit(&lock->servingTicket, 1, memory_order_seq_cst);

    interrupts_enable();
}