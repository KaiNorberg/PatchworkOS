#pragma once

#include <kernel/sync/lock.h>

#include <stdatomic.h>

/**
 * @brief Sequence Lock
 * @defgroup kernel_sync_seqlock Seqlock
 * @ingroup kernel_sync
 *
 * A sequence lock is similar to a read-write lock, but optimized for scenarios where there are many more readers than
 * writers, or where reads are frequent and writes are rare.
 *
 * Readers can read the data without acquiring a lock, instead they check a "sequence number" before and after their
 * read to verify that no write occured during their read, if one did, they must retry their read.
 *
 * Writers acquire the lock exclusively and increment the "sequence number" before and after their write, this means
 * that readers can detect if a write has occured, and if a write is currently in progress by checking if the sequence
 * number is odd.
 *
 * @{
 */

/**
 * @brief Sequence lock structure.
 * @struct seqlock_t
 */
typedef struct
{
    atomic_uint64_t sequence;
    lock_t writeLock;
} seqlock_t;

/**
 * @brief Create a sequence lock initializer.
 *
 * @return A `seqlock_t` initializer.
 */
#define SEQLOCK_CREATE() {.sequence = ATOMIC_VAR_INIT(0), .writeLock = LOCK_CREATE()}

/**
 * @brief Initializes a sequence lock.
 *
 * @param seqlock Pointer to the sequence lock to initialize.
 */
static inline void seqlock_init(seqlock_t* seqlock)
{
    atomic_init(&seqlock->sequence, 0);
    lock_init(&seqlock->writeLock);
}

/**
 * @brief Acquires the write lock of a sequence lock.
 *
 * This function busy-waits until the write lock is acquired.
 *
 * @param seqlock Pointer to the sequence lock.
 */
static inline void seqlock_write_acquire(seqlock_t* seqlock)
{
    lock_acquire(&seqlock->writeLock);
    atomic_fetch_add_explicit(&seqlock->sequence, 1, memory_order_acquire);
}

/**
 * @brief Releases the write lock of a sequence lock.
 *
 * @param seqlock Pointer to the sequence lock.
 */
static inline void seqlock_write_release(seqlock_t* seqlock)
{
    atomic_fetch_add_explicit(&seqlock->sequence, 1, memory_order_release);
    lock_release(&seqlock->writeLock);
}

/**
 * @brief Begins a read operation on a sequence lock.
 *
 * Should be called in a loop, for example:
 * ```c
 * uint64_t seq;
 * do {
 *    seq = seqlock_read_begin(&seqlock);
 *   // read data here
 * } while (seqlock_read_retry(&seqlock, seq));
 * ```
 *
 * Or use the `SEQLOCK_READ_SCOPE()` macro.
 *
 * @param seqlock Pointer to the sequence lock.
 * @return The current sequence number.
 */
static inline uint64_t seqlock_read_begin(seqlock_t* seqlock)
{
    return atomic_load_explicit(&seqlock->sequence, memory_order_acquire);
}

/**
 * @brief Checks if a read operation on a sequence lock needs to be retried.
 *
 * @param seqlock Pointer to the sequence lock.
 * @param seq The sequence number returned by `seqlock_read_begin()`.
 * @return `true` if the read operation needs to be retried, `false` otherwise.
 */
static inline bool seqlock_read_retry(seqlock_t* seqlock, uint64_t seq)
{
    atomic_thread_fence(memory_order_acquire);
    return (atomic_load_explicit(&seqlock->sequence, memory_order_relaxed) != seq) || (seq & 1);
}

/**
 * @brief Read scope for a sequence lock.
 *
 * Example usage:
 * ```c
 * SEQLOCK_READ_SCOPE(&seqlock)
 * {
 *     // read data here
 * }
 * ```
 *
 * @param seqlock Pointer to the sequence lock.
 */
#define SEQLOCK_READ_SCOPE(seqlock) \
    for (uint64_t __seq = seqlock_read_begin(seqlock); \
        seqlock_read_retry(seqlock, __seq) ? (__seq = seqlock_read_begin(seqlock), true) : false;)

/** @} */
