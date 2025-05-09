#pragma once

#include <stdatomic.h>

#include "defs.h"
#include "trap.h"

typedef struct
{
    atomic_uint_fast16_t nextTicket;
    atomic_uint_fast16_t nowServing;
} lock_t;

#define LOCK_DEFER(lock) \
    __attribute__((cleanup(lock_cleanup))) lock_t* CONCAT(l, __COUNTER__) = (lock); \
    lock_acquire((lock))

static inline void lock_init(lock_t* lock)
{
    atomic_init(&lock->nextTicket, 0);
    atomic_init(&lock->nowServing, 0);
}

static inline void lock_acquire(lock_t* lock)
{
    cli_push();

    // Overflow does not matter
    uint_fast16_t ticket = atomic_fetch_add_explicit(&lock->nextTicket, 1, memory_order_relaxed);
    while (atomic_load_explicit(&lock->nowServing, memory_order_acquire) != ticket)
    {
        asm volatile("pause");
    }
}

static inline void lock_release(lock_t* lock)
{
    atomic_fetch_add_explicit(&lock->nowServing, 1, memory_order_release);
    cli_pop();
}

static inline void lock_cleanup(lock_t** lock)
{
    lock_release(*lock);
}
