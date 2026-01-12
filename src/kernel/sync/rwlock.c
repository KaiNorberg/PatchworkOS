#include <kernel/cpu/cli.h>
#include <kernel/sync/rwlock.h>

#ifndef NDEBUG
#include <kernel/log/panic.h>
#include <kernel/sched/timer.h>
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
    cli_push();

#ifndef NDEBUG
    uint64_t iterations = 0;
#endif

    uint_fast16_t ticket = atomic_fetch_add_explicit(&lock->readTicket, 1, memory_order_relaxed);

    while (atomic_load_explicit(&lock->readServe, memory_order_acquire) != ticket)
    {
        asm volatile("pause");
#ifndef NDEBUG
        if (++iterations >= RWLOCK_DEADLOCK_ITERATIONS)
        {
            panic(NULL, "Deadlock in rwlock_read_acquire detected");
        }
#endif
    }

    while (atomic_load_explicit(&lock->writeServe, memory_order_relaxed) !=
        atomic_load_explicit(&lock->writeTicket, memory_order_relaxed))
    {
#ifndef NDEBUG
        if (++iterations >= RWLOCK_DEADLOCK_ITERATIONS)
        {
            panic(NULL, "Deadlock in rwlock_read_acquire detected");
        }
#endif
        asm volatile("pause");
    }

    atomic_fetch_add_explicit(&lock->activeReaders, 1, memory_order_acquire);
}

void rwlock_read_release(rwlock_t* lock)
{
    atomic_fetch_sub_explicit(&lock->activeReaders, 1, memory_order_release);
    atomic_fetch_add_explicit(&lock->readServe, 1, memory_order_release);

    cli_pop();
}

void rwlock_write_acquire(rwlock_t* lock)
{
    cli_push();

#ifndef NDEBUG
    uint64_t iterations = 0;
#endif

    uint_fast16_t ticket = atomic_fetch_add_explicit(&lock->writeTicket, 1, memory_order_relaxed);

    while (atomic_load_explicit(&lock->writeServe, memory_order_acquire) != ticket)
    {
#ifndef NDEBUG
        if (++iterations >= RWLOCK_DEADLOCK_ITERATIONS)
        {
            panic(NULL, "Deadlock in rwlock_write_acquire detected");
        }
#endif
        asm volatile("pause");
    }

    while (atomic_load_explicit(&lock->activeReaders, memory_order_acquire) > 0)
    {
#ifndef NDEBUG
        if (++iterations >= RWLOCK_DEADLOCK_ITERATIONS)
        {
            panic(NULL, "Deadlock in rwlock_write_acquire detected");
        }
#endif
        asm volatile("pause");
    }

    bool expected = false;
    while (!atomic_compare_exchange_weak_explicit(&lock->activeWriter, &expected, true, memory_order_acquire,
        memory_order_relaxed))
    {
#ifndef NDEBUG
        if (++iterations >= RWLOCK_DEADLOCK_ITERATIONS)
        {
            panic(NULL, "Deadlock in rwlock_write_acquire detected");
        }
#endif
        expected = false;
        asm volatile("pause");
    }
}

void rwlock_write_release(rwlock_t* lock)
{
    atomic_store_explicit(&lock->activeWriter, false, memory_order_release);
    atomic_fetch_add_explicit(&lock->writeServe, 1, memory_order_release);

    cli_pop();
}
