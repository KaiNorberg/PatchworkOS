#pragma once

#include "sched/wait.h"
#include <stdint.h>

typedef struct rwmutex rwmutex_t;

/**
 * @brief Read-Write Mutex
 * @defgroup kernel_sync_rwmutex Read-Write Mutex
 * @ingroup kernel_sync
 *
 * @{
 */

/**
 * @brief Acquires a rwmutex for reading for the reminder of the current scope.
 *
 * @param mutex Pointer to the rwmutex to acquire.
 */
#define RWMUTEX_READ_SCOPE(mutex) \
    __attribute__((cleanup(rwmutex_read_cleanup))) rwmutex_t* CONCAT(rm, __COUNTER__) = (mutex); \
    rwmutex_read_acquire((mutex))

/**
 * @brief Acquires a rwmutex for writing for the reminder of the current scope.
 *
 * @param mutex Pointer to the rwmutex to acquire.
 */
#define RWMUTEX_WRITE_SCOPE(mutex) \
    __attribute__((cleanup(rwmutex_write_cleanup))) rwmutex_t* CONCAT(wm, __COUNTER__) = (mutex); \
    rwmutex_write_acquire((mutex))

/**
 * @brief Maximum amount of rwmutexes that a thread can acquire simultaneously.
 */
#define RWMUTEX_MAX_MUTEXES 16

/**
 * @brief Per-Thread Read-Write Mutex context entry structure.
 * @struct rwmutex_ctx_entry_t
 *
 * Tracks the read and write depth for a specific rwmutex acquired by the thread.
 */
typedef struct
{
    rwmutex_t* mutex;
    uint16_t readDepth;
    uint16_t writeDepth;
} rwmutex_ctx_entry_t;

/**
 * @brief Per-Thread Read-Write Mutex context structure.
 * @struct rwmutex_ctx_t
 *
 * Allows for recursive locking by tracking what mutexes are currently acquired by the thread.
 */
typedef struct
{
    rwmutex_ctx_entry_t entries[RWMUTEX_MAX_MUTEXES];
} rwmutex_ctx_t;

/**
 * @brief Read-Write Mutex structure.
 * @struct rwmutex_t
 *
 * A Read-Write Mutex allows one only writer or multiple readers to access a shared resource at the same time. This
 * implementation prioritizes writers over readers and supports recursive locking.
 *
 * A writer can recursively acquire the mutex for either reading or writing without much worry about deadlocks, however
 * while a reader can recursively acquire the mutex for either reading or writing, its important to be careful to avoid
 * deadlocks when upgrading from a read lock to a write lock as if multiple readers try to upgrade at the same time they
 * will all deadlock waiting to be the last reader.
 */
typedef struct rwmutex
{
    uint16_t activeReaders;
    uint16_t waitingWriters;
    wait_queue_t readerQueue;
    wait_queue_t writerQueue;
    bool hasWriter;
    bool isUpgradingReader; ///< Used to check for deadlocks by having multiple readers try to upgrade at the same time.
    lock_t lock;
} rwmutex_t;

/**
 * @brief Initializes a rwmutex context.
 *
 * @param ctx Pointer to the rwmutex context to initialize.
 */
void rwmutex_ctx_init(rwmutex_ctx_t* ctx);

/**
 * @brief Deinitializes a rwmutex context.
 *
 * @param ctx Pointer to the rwmutex context to deinitialize.
 */
void rwmutex_ctx_deinit(rwmutex_ctx_t* ctx);

/**
 * @brief Initializes a rwmutex.
 *
 * @param mtx Pointer to the rwmutex to initialize.
 */
void rwmutex_init(rwmutex_t* mtx);

/**
 * @brief Deinitializes a rwmutex.
 *
 * @param mtx Pointer to the rwmutex to deinitialize.
 */
void rwmutex_deinit(rwmutex_t* mtx);

/**
 * @brief Acquires a rwmutex for reading, blocking until it is available.
 *
 * @param mtx Pointer to the rwmutex to acquire.
 */
void rwmutex_read_acquire(rwmutex_t* mtx);

/**
 * @brief Tries to acquire a rwmutex for reading.
 *
 * If the rwmutex is owned by another thread for writing or a writer is waiting, this function
 * will fail with `EWOULDBLOCK`.
 *
 * If the function succeeds, `rwmutex_read_release()` must be called to release the rwmutex.
 *
 * @param mtx Pointer to the rwmutex to acquire.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t rwmutex_read_try_acquire(rwmutex_t* mtx);

/**
 * @brief Releases a rwmutex from reading.
 *
 * @param mtx Pointer to the rwmutex to release.
 */
void rwmutex_read_release(rwmutex_t* mtx);

/**
 * @brief Acquires a rwmutex for writing, blocking until it is available.
 *
 * @param mtx Pointer to the rwmutex to acquire.
 */
void rwmutex_write_acquire(rwmutex_t* mtx);

/**
 * @brief Tries to acquire a rwmutex for writing.
 *
 * If the function succeeds, `rwmutex_write_release()` must be called to release the rwmutex.
 *
 * @param mtx Pointer to the rwmutex to acquire.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t rwmutex_write_try_acquire(rwmutex_t* mtx);

/**
 * @brief Acquires a rwmutex for writing, without blocking.
 *
 * This function will spin until the rwmutex is acquired making it useful but not ideal for acquiring rwmutexes in a
 * interrupt handler.
 *
 * @param mtx Pointer to the rwmutex to acquire.
 */
void rwmutex_write_spin_acquire(rwmutex_t* mtx);

/**
 * @brief Releases a rwmutex from writing.
 *
 * @param mtx Pointer to the rwmutex to release.
 */
void rwmutex_write_release(rwmutex_t* mtx);

static inline void rwmutex_read_cleanup(rwmutex_t** mutex)
{
    rwmutex_read_release(*mutex);
}

static inline void rwmutex_write_cleanup(rwmutex_t** mutex)
{
    rwmutex_write_release(*mutex);
}

/** @} */
