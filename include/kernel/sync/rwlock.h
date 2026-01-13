#pragma once

#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/cli.h>

#include <sys/defs.h>
#include <stdatomic.h>

#ifndef NDEBUG
#include <kernel/log/panic.h>
#include <kernel/sched/timer.h>
#endif

typedef struct thread thread_t;

/**
 * @brief Read-Write Ticket Lock
 * @defgroup kernel_sync_rwlock Read-Write Ticket Lock
 * @ingroup kernel_sync
 *
 * @{
 */

/**
 * @brief Number of iterations before we consider a deadlock to have occurred in a rwlock operation.
 * This is only used in debug builds.
 */
#define RWLOCK_DEADLOCK_ITERATIONS 10000000

/**
 * @brief Acquires a rwlock for reading for the reminder of the current scope.
 *
 * @param lock Pointer to the rwlock to acquire.
 */
#define RWLOCK_READ_SCOPE(lock) \
    __attribute__((cleanup(rwlock_read_cleanup))) rwlock_t* CONCAT(rl, __COUNTER__) = (lock); \
    rwlock_read_acquire((lock))

/**
 * @brief Acquires a rwlock for writing for the reminder of the current scope.
 *
 * @param lock Pointer to the rwlock to acquire.
 */
#define RWLOCK_WRITE_SCOPE(lock) \
    __attribute__((cleanup(rwlock_write_cleanup))) rwlock_t* CONCAT(wl, __COUNTER__) = (lock); \
    rwlock_write_acquire((lock))

/**
 * @brief Create a rwlock initializer.
 *
 * @return A rwlock_t initializer.
 */
#define RWLOCK_CREATE() \
    (rwlock_t) \
    { \
        .readTicket = ATOMIC_VAR_INIT(0), .readServe = ATOMIC_VAR_INIT(0), .writeTicket = ATOMIC_VAR_INIT(0), \
        .writeServe = ATOMIC_VAR_INIT(0), .activeReaders = ATOMIC_VAR_INIT(0) \
    }

/**
 * @brief Read-Write Ticket Lock structure.
 * @struct rwlock_t
 *
 * A Read-Write Ticket Lock allows one only writer or multiple readers to access a shared resource at the same time.
 */
typedef struct
{
    atomic_uint16_t readTicket;
    atomic_uint16_t readServe;
    atomic_uint16_t writeTicket;
    atomic_uint16_t writeServe;
    atomic_uint16_t activeReaders;
} rwlock_t;

/**
 * @brief Initializes a rwlock.
 *
 * @param lock Pointer to the rwlock to initialize.
 */
static inline void rwlock_init(rwlock_t* lock)
{
    atomic_init(&lock->readTicket, 0);
    atomic_init(&lock->readServe, 0);
    atomic_init(&lock->writeTicket, 0);
    atomic_init(&lock->writeServe, 0);
    atomic_init(&lock->activeReaders, 0);
}

/**
 * @brief Acquires a rwlock for reading, blocking until it is available.
 *
 * @param lock Pointer to the rwlock to acquire.
 */
static inline void rwlock_read_acquire(rwlock_t* lock)
{
    cli_push();

#ifndef NDEBUG
    uint64_t iterations = 0;
#endif

    uint16_t ticket = atomic_fetch_add_explicit(&lock->readTicket, 1, memory_order_relaxed);

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
    atomic_fetch_add_explicit(&lock->readServe, 1, memory_order_release);
}

/**
 * @brief Releases a rwlock from reading.
 *
 * @param lock Pointer to the rwlock to release.
 */
static inline void rwlock_read_release(rwlock_t* lock)
{
    atomic_fetch_sub_explicit(&lock->activeReaders, 1, memory_order_release);

    cli_pop();
}

/**
 * @brief Acquires a rwlock for writing, blocking until it is available.
 *
 * @param lock Pointer to the rwlock to acquire.
 */
static inline void rwlock_write_acquire(rwlock_t* lock)
{
    cli_push();

#ifndef NDEBUG
    uint64_t iterations = 0;
#endif

    uint16_t ticket = atomic_fetch_add_explicit(&lock->writeTicket, 1, memory_order_relaxed);

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
}

/**
 * @brief Releases a rwlock from writing.
 *
 * @param lock Pointer to the rwlock to release.
 */
static inline void rwlock_write_release(rwlock_t* lock)
{
    atomic_fetch_add_explicit(&lock->writeServe, 1, memory_order_release);

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

/** @} */
