#pragma once

#include "cpu/trap.h"
#ifndef NDEBUG
#include "log/panic.h"
#include "sched/timer.h"
#endif

#include <common/defs.h>
#include <stdatomic.h>

#define LOCK_DEADLOCK_TIMEOUT (CLOCKS_PER_SEC / 10)

typedef struct
{
    atomic_uint_fast16_t nextTicket;
    atomic_uint_fast16_t nowServing;
} lock_t;

#define LOCK_SCOPE(lock) \
    __attribute__((cleanup(lock_cleanup))) lock_t* CONCAT(l, __COUNTER__) = (lock); \
    lock_acquire((lock))

#define LOCK_CREATE \
    (lock_t) \
    { \
        .nextTicket = ATOMIC_VAR_INIT(0), .nowServing = ATOMIC_VAR_INIT(0), \
    }

static inline void lock_init(lock_t* lock)
{
    atomic_init(&lock->nextTicket, 0);
    atomic_init(&lock->nowServing, 0);
}

static inline void lock_acquire(lock_t* lock)
{
    cli_push();

#ifndef NDEBUG
    clock_t start = timer_uptime();
#endif

    // Overflow does not matter
    uint_fast16_t ticket = atomic_fetch_add_explicit(&lock->nextTicket, 1, memory_order_relaxed);
    while (atomic_load_explicit(&lock->nowServing, memory_order_acquire) != ticket)
    {
        asm volatile("pause");

#ifndef NDEBUG
        clock_t now = timer_uptime();
        if (start != 0 && now - start > LOCK_DEADLOCK_TIMEOUT)
        {
            panic(NULL, "Deadlock in lock_acquire detected");
        }
#endif
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
