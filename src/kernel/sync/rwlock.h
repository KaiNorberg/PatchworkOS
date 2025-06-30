#pragma once

#include <stdatomic.h>

#include "cpu/trap.h"

typedef struct
{
    atomic_uint_fast16_t readTicket;
    atomic_uint_fast16_t readServe;
    atomic_uint_fast16_t writeTicket;
    atomic_uint_fast16_t writeServe;
    atomic_uint_fast16_t activeReaders;
    atomic_bool activeWriter;
} rwlock_t;

#define RWLOCK_READ_DEFER(lock) \
    __attribute__((cleanup(rwlock_read_cleanup))) rwlock_t* CONCAT(rl, __COUNTER__) = (lock); \
    rwlock_read_acquire((lock))

#define RWLOCK_WRITE_DEFER(lock) \
    __attribute__((cleanup(rwlock_write_cleanup))) rwlock_t* CONCAT(wl, __COUNTER__) = (lock); \
    rwlock_write_acquire((lock))

static inline void rwlock_init(rwlock_t* lock)
{
    atomic_init(&lock->readTicket, 0);
    atomic_init(&lock->readServe, 0);
    atomic_init(&lock->writeTicket, 0);
    atomic_init(&lock->writeServe, 0);
    atomic_init(&lock->activeReaders, 0);
    atomic_init(&lock->activeWriter, false);
}

static inline void rwlock_read_acquire(rwlock_t* lock)
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

static inline void rwlock_read_release(rwlock_t* lock)
{
    atomic_fetch_sub(&lock->activeReaders, 1);
    atomic_fetch_add(&lock->readServe, 1);

    cli_pop();
}

static inline void rwlock_upgrade_read_to_write(rwlock_t* lock)
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

static inline void rwlock_write_acquire(rwlock_t* lock)
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

static inline void rwlock_write_release(rwlock_t* lock)
{
    atomic_store(&lock->activeWriter, false);
    atomic_fetch_add(&lock->writeServe, 1);

    cli_pop();
}

static inline void rwlock_read_cleanup(rwlock_t** lock)
{
    rwlock_read_release(*lock);
}

static inline void rwlock_write_cleanup(rwlock_t** lock)
{
    rwlock_write_release(*lock);
}
