#include "lock.h"

typedef atomic_flag Lock;

atomic_size_t lockCounter;

void lock_init()
{
    atomic_init(&lockCounter, 0);
}

Lock lock_new()
{
    return (Lock)ATOMIC_FLAG_INIT;
}

void lock_acquire(Lock* lock)
{
    if (atomic_fetch_add(&lockCounter, 1) == 0)
    {
        asm volatile("cli");
    }

    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire))
    {
        asm volatile("pause");
    }
}

void lock_release(Lock* lock)
{
    atomic_flag_clear_explicit(lock, memory_order_release);

    if (atomic_fetch_sub(&lockCounter, 1) == 1)
    {
        asm volatile("sti");
    }
}