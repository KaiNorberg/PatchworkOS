#include "lock.h"

#include "interrupts/interrupts.h"

Lock lock_new()
{
    return (Lock)ATOMIC_FLAG_INIT;
}

void lock_acquire(Lock* lock)
{
    interrupts_disable();

    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire))
    {
        asm volatile("pause");
    }
}

void lock_release(Lock* lock)
{
    atomic_flag_clear_explicit(lock, memory_order_release);

    interrupts_enable();
}