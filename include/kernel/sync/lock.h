#pragma once

#include <kernel/cpu/interrupt.h>

#ifndef NDEBUG
#include <kernel/log/panic.h>
#endif

#include <kernel/defs.h>
#include <kernel/drivers/com.h>
#include <stdatomic.h>

/**
 * @brief Ticket spinlock.
 * @defgroup kernel_sync_lock Lock
 * @ingroup kernel_sync
 *
 * @{
 */

/**
 * @brief Number of iterations before we consider a deadlock to have occurred in lock_acquire.
 * This is only used in debug builds.
 */
#define LOCK_DEADLOCK_ITERATIONS 10000000

/**
 * @brief Lock canary value to detect memory corruption.
 */
#define LOCK_CANARY 0xDEADBEEF

/**
 * @brief A simple ticket lock implementation.
 *
 * This lock disables interrupts when acquired, and restores the interrupt state when released.
 * It is not recursive, and attempting to acquire a lock that is already held by the same CPU will
 * result in a deadlock.
 *
 * In debug builds, the lock contains a canary value to detect memory corruption and a deadlock detection
 * mechanism that will panic if a deadlock is detected.
 */
typedef struct
{
    atomic_uint16_t nextTicket;
    atomic_uint16_t nowServing;
#ifndef NDEBUG
    uint32_t canary;
    uintptr_t calledFrom;
#endif
} lock_t;

/**
 * @brief Acquires a lock for the reminder of the current scope.
 *
 * @param lock Pointer to the lock to acquire.
 */
#define LOCK_SCOPE(lock) \
    __attribute__((cleanup(lock_cleanup))) lock_t* CONCAT(l, __COUNTER__) = (lock); \
    lock_acquire((lock))

/**
 * @brief Create a lock initializer.
 * @macro LOCK_CREATE
 *
 * @return A `lock_t` initializer.
 */
#ifndef NDEBUG
#define LOCK_CREATE (lock_t){.nextTicket = ATOMIC_VAR_INIT(0), .nowServing = ATOMIC_VAR_INIT(0), .canary = 0xDEADBEEF}
#else
#define LOCK_CREATE \
    (lock_t) \
    { \
        .nextTicket = ATOMIC_VAR_INIT(0), .nowServing = ATOMIC_VAR_INIT(0) \
    }
#endif

/**
 * @brief Initializes a lock.
 *
 * @param lock Pointer to the lock to initialize.
 */
static inline void lock_init(lock_t* lock)
{
    atomic_init(&lock->nextTicket, 0);
    atomic_init(&lock->nowServing, 0);
#ifndef NDEBUG
    lock->canary = LOCK_CANARY;
#endif
}

/**
 * @brief Acquires a lock, blocking until it is available.
 *
 * This function disables interrupts on the current CPU.
 * It is not recursive, and attempting to acquire a lock that is already held by the same CPU will result in a deadlock.
 *
 * @param lock Pointer to the lock to acquire.
 */
static inline void lock_acquire(lock_t* lock)
{
    interrupt_disable();

#ifndef NDEBUG
    if (lock->canary != LOCK_CANARY)
    {
        interrupt_enable();
        panic(NULL, "Lock canary corrupted");
    }
    lock->calledFrom = (uintptr_t)__builtin_return_address(0);
    uint64_t iterations = 0;
#endif

    uint16_t ticket = atomic_fetch_add_explicit(&lock->nextTicket, 1, memory_order_relaxed);
    while (atomic_load_explicit(&lock->nowServing, memory_order_acquire) != ticket)
    {
        asm volatile("pause");

#ifndef NDEBUG
        if (lock->canary != LOCK_CANARY)
        {
            interrupt_enable();
            panic(NULL, "Lock canary corrupted after %d iterations", iterations);
        }
        if (++iterations >= LOCK_DEADLOCK_ITERATIONS)
        {
            interrupt_enable();
            panic(NULL, "Deadlock detected in lock last acquired from %p", (void*)lock->calledFrom);
        }
#endif
    }

    atomic_thread_fence(memory_order_seq_cst);
}

/**
 * @brief Releases a lock.
 *
 * This function restores the interrupt state on the current CPU to what it was before the lock was acquired.
 *
 * @param lock Pointer to the lock to release.
 */
static inline void lock_release(lock_t* lock)
{
#ifndef NDEBUG
    if (lock->canary != LOCK_CANARY)
    {
        panic(NULL, "Lock canary corrupted");
    }
#endif

    atomic_fetch_add_explicit(&lock->nowServing, 1, memory_order_release);
    interrupt_enable();
}

static inline void lock_cleanup(lock_t** lock)
{
    lock_release(*lock);
}

/** @} */
