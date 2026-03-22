#pragma once

#include <kernel/sched/wait.h>
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
 * @brief Read-Write Mutex structure.
 * @struct rwmutex_t
 *
 * A Read-Write Mutex allows one only writer or multiple readers to access a shared resource at the same time. This
 * implementation prioritizes writers over readers and does not support recursive locking.
 */
typedef struct rwmutex
{
    uint16_t activeReaders;
    uint16_t waitingWriters;
    wait_queue_t readerQueue;
    wait_queue_t writerQueue;
    bool hasWriter;
    lock_t lock;
} rwmutex_t;

/**
 * @brief Create a rwmutex initializer.
 * @def RWMUTEX_CREATE
 *
 * @param name The name of the rwmutex variable to initialize.
 * @return A `rwmutex_t` initializer.
 */
#define RWMUTEX_CREATE(name) \
    { \
        .activeReaders = 0, \
        .waitingWriters = 0, \
        .readerQueue = WAIT_QUEUE_CREATE(name.readerQueue), \
        .writerQueue = WAIT_QUEUE_CREATE(name.writerQueue), \
        .hasWriter = false, \
        .lock = LOCK_CREATE(), \
    }

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
 * If the function succeeds, `rwmutex_read_release()` must be called to release the rwmutex.
 *
 * @param mtx Pointer to the rwmutex to acquire.
 * @return `true` if the mutex was acquired, `false` otherwise.
 */
bool rwmutex_read_try_acquire(rwmutex_t* mtx);

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
 * @return `true` if the mutex was acquired, `false` otherwise.
 */
bool rwmutex_write_try_acquire(rwmutex_t* mtx);

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
