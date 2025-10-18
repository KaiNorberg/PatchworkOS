#pragma once

#include <stdatomic.h>

#include "cpu/interrupt.h"

typedef struct thread thread_t;

/**
 * @brief Read-Write Ticket Lock
 * @defgroup kernel_sync_rwlock Read-Write Ticket Lock
 * @ingroup kernel_sync
 *
 * @{
 */

/**
 * @brief Maximum time before we consider a deadlock to have occurred.
 */
#define RWLOCK_DEADLOCK_TIMEOUT (CLOCKS_PER_SEC * 10)

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
#define RWLOCK_CREATE \
    (rwlock_t){.readTicket = ATOMIC_VAR_INIT(0), \
        .readServe = ATOMIC_VAR_INIT(0), \
        .writeTicket = ATOMIC_VAR_INIT(0), \
        .writeServe = ATOMIC_VAR_INIT(0), \
        .activeReaders = ATOMIC_VAR_INIT(0), \
        .activeWriter = ATOMIC_VAR_INIT(false)}

/**
 * @brief Read-Write Ticket Lock structure.
 * @struct rwlock_t
 *
 * A Read-Write Ticket Lock allows one only writer or multiple readers to access a shared resource at the same time.
 */
typedef struct
{
    atomic_uint_fast16_t readTicket;
    atomic_uint_fast16_t readServe;
    atomic_uint_fast16_t writeTicket;
    atomic_uint_fast16_t writeServe;
    atomic_uint_fast16_t activeReaders;
    atomic_bool activeWriter;
} rwlock_t;

/**
 * @brief Initializes a rwlock.
 *
 * @param lock Pointer to the rwlock to initialize.
 */
void rwlock_init(rwlock_t* lock);

/**
 * @brief Acquires a rwlock for reading, blocking until it is available.
 *
 * @param lock Pointer to the rwlock to acquire.
 */
void rwlock_read_acquire(rwlock_t* lock);

/**
 * @brief Releases a rwlock from reading.
 *
 * @param lock Pointer to the rwlock to release.
 */
void rwlock_read_release(rwlock_t* lock);

/**
 * @brief Acquires a rwlock for writing, blocking until it is available.
 *
 * @param lock Pointer to the rwlock to acquire.
 */
void rwlock_write_acquire(rwlock_t* lock);

/**
 * @brief Releases a rwlock from writing.
 *
 * @param lock Pointer to the rwlock to release.
 */
void rwlock_write_release(rwlock_t* lock);

static inline void rwlock_read_cleanup(rwlock_t** lock)
{
    rwlock_read_release(*lock);
}

static inline void rwlock_write_cleanup(rwlock_t** lock)
{
    rwlock_write_release(*lock);
}

/** @} */
