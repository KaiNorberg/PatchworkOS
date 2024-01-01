#include "spin_lock.h"

SpinLock spin_lock_new()
{
    SpinLock newLock = ATOMIC_FLAG_INIT;
    return newLock;
}

void spin_lock_acquire(SpinLock* lock)
{
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire))
    {
        __builtin_ia32_pause();
    }
}

void spin_lock_release(SpinLock* lock)
{
    atomic_flag_clear_explicit(lock, memory_order_release);
}