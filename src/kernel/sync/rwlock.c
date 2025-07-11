#include "rwlock.h"

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

    uint_fast16_t ticket = atomic_fetch_add(&lock->readTicket, 1);

    while (atomic_load(&lock->readServe) != ticket)
    {
        asm volatile("pause");
    }

    while (atomic_load(&lock->writeServe) != atomic_load(&lock->writeTicket))
    {
        asm volatile("pause");
    }

    atomic_fetch_add(&lock->activeReaders, 1);
}

void rwlock_read_release(rwlock_t* lock)
{
    atomic_fetch_sub(&lock->activeReaders, 1);
    atomic_fetch_add(&lock->readServe, 1);

    cli_pop();
}

void rwlock_upgrade_read_to_write(rwlock_t* lock)
{
    atomic_fetch_sub(&lock->activeReaders, 1);
    atomic_fetch_add(&lock->readServe, 1);

    uint_fast16_t writeTicket = atomic_fetch_add(&lock->writeTicket, 1);

    while (atomic_load(&lock->writeServe) != writeTicket)
    {
        asm volatile("pause");
    }

    while (atomic_load(&lock->activeReaders) > 0)
    {
        asm volatile("pause");
    }

    bool expected = false;
    while (!atomic_compare_exchange_weak(&lock->activeWriter, &expected, true))
    {
        expected = false;
        asm volatile("pause");
    }
}

void rwlock_write_acquire(rwlock_t* lock)
{
    cli_push();

    uint_fast16_t ticket = atomic_fetch_add(&lock->writeTicket, 1);

    while (atomic_load(&lock->writeServe) != ticket)
    {
        asm volatile("pause");
    }

    while (atomic_load(&lock->activeReaders) > 0)
    {
        asm volatile("pause");
    }

    bool expected = false;
    while (!atomic_compare_exchange_weak(&lock->activeWriter, &expected, true))
    {
        expected = false;
        asm volatile("pause");
    }
}

void rwlock_write_release(rwlock_t* lock)
{
    atomic_store(&lock->activeWriter, false);
    atomic_fetch_add(&lock->writeServe, 1);

    cli_pop();
}
