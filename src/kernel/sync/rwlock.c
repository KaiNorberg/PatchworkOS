#include "rwlock.h"

#ifndef NDEBUG
#include "log/panic.h"
#include "sched/timer.h"
#endif

void rwlock_init(rwlock_t* lock)
{
    atomic_init(&lock->readTicket, 0);
    atomic_init(&lock->readServe, 0);
    atomic_init(&lock->writeTicket, 0);
    atomic_init(&lock->writeServe, 0);
    atomic_init(&lock->activeReaders, 0);
    atomic_init(&lock->activeWriter, false);
}

void rwlock_read_acquire(rwlock_t* lock)
{
    interrupt_disable();

#ifndef NDEBUG
    clock_t start = timer_uptime();
#endif

    uint_fast16_t ticket = atomic_fetch_add(&lock->readTicket, 1);

    while (atomic_load(&lock->readServe) != ticket)
    {
        asm volatile("pause");
#ifndef NDEBUG
        clock_t now = timer_uptime();
        if (start != 0 && now - start > RWLOCK_DEADLOCK_TIMEOUT)
        {
            panic(NULL, "Deadlock in rwlock_read_acquire detected");
        }
#endif
    }

    while (atomic_load(&lock->writeServe) != atomic_load(&lock->writeTicket))
    {
#ifndef NDEBUG
        clock_t now = timer_uptime();
        if (start != 0 && now - start > RWLOCK_DEADLOCK_TIMEOUT)
        {
            panic(NULL, "Deadlock in rwlock_read_acquire detected");
        }
#endif
        asm volatile("pause");
    }

    atomic_fetch_add(&lock->activeReaders, 1);

    atomic_thread_fence(memory_order_seq_cst);
}

void rwlock_read_release(rwlock_t* lock)
{
    atomic_fetch_sub(&lock->activeReaders, 1);
    atomic_fetch_add(&lock->readServe, 1);

    interrupt_enable();
}

void rwlock_write_acquire(rwlock_t* lock)
{
    interrupt_disable();

#ifndef NDEBUG
    clock_t start = timer_uptime();
#endif

    uint_fast16_t ticket = atomic_fetch_add(&lock->writeTicket, 1);

    while (atomic_load(&lock->writeServe) != ticket)
    {
#ifndef NDEBUG
        clock_t now = timer_uptime();
        if (start != 0 && now - start > RWLOCK_DEADLOCK_TIMEOUT)
        {
            panic(NULL, "Deadlock in rwlock_write_acquire detected");
        }
#endif
        asm volatile("pause");
    }

    while (atomic_load(&lock->activeReaders) > 0)
    {
#ifndef NDEBUG
        clock_t now = timer_uptime();
        if (start != 0 && now - start > RWLOCK_DEADLOCK_TIMEOUT)
        {
            panic(NULL, "Deadlock in rwlock_write_acquire detected");
        }
#endif
        asm volatile("pause");
    }

    bool expected = false;
    while (!atomic_compare_exchange_weak(&lock->activeWriter, &expected, true))
    {
#ifndef NDEBUG
        clock_t now = timer_uptime();
        if (start != 0 && now - start > RWLOCK_DEADLOCK_TIMEOUT)
        {
            panic(NULL, "Deadlock in rwlock_write_acquire detected");
        }
#endif
        expected = false;
        asm volatile("pause");
    }

    atomic_thread_fence(memory_order_seq_cst);
}

void rwlock_write_release(rwlock_t* lock)
{
    atomic_store(&lock->activeWriter, false);
    atomic_fetch_add(&lock->writeServe, 1);

    interrupt_enable();
}
