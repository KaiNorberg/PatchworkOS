#pragma once

#include <stdatomic.h>

#include "defs.h"
#include "trap.h"

typedef struct
{
    atomic_uint16 nextTicket;
    atomic_uint16 nowServing;
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
    uint32_t ticket = atomic_fetch_add(&lock->nextTicket, 1);
    while (atomic_load(&lock->nowServing) != ticket)
    {
        asm volatile("pause");
    }
}

static inline void lock_release(lock_t* lock)
{
    atomic_fetch_add(&lock->nowServing, 1);

    cli_pop();
}

static inline void lock_cleanup(lock_t** lock)
{
    lock_release(*lock);
}
