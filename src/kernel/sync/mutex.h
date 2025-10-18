#pragma once

#include "sched/wait.h"
#include "sync/lock.h"

#include <stdbool.h>

typedef struct thread thread_t;

/**
 * @brief Recursive Mutex
 * @defgroup kernel_sync_mutex Mutex
 * @ingroup kernel_sync
 *
 * @{
 */

/**
 * @brief Acquires a mutex for the reminder of the current scope.
 *
 * @param mutex Pointer to the mutex to acquire.
 */
#define MUTEX_SCOPE(mutex) \
    __attribute__((cleanup(mutex_cleanup))) mutex_t* CONCAT(m, __COUNTER__) = (mutex); \
    mutex_acquire((mutex))

/**
 * @brief Create a mutex initializer.
 *
 * @return A mutex initializer.
 */
#define MUTEX_CREATE {.waitQueue = WAIT_QUEUE_CREATE, .owner = NULL, .depth = 0, .lock = LOCK_CREATE}

/**
 * @brief Mutex structure.
 * @struct mutex_t
 */
typedef struct
{
    wait_queue_t waitQueue;
    thread_t* owner;
    uint32_t depth;
    lock_t lock;
} mutex_t;

/**
 * @brief Initializes a mutex.
 *
 * @param mtx Pointer to the mutex to initialize.
 */
void mutex_init(mutex_t* mtx);

/**
 * @brief Deinitializes a mutex.
 *
 * @param mtx Pointer to the mutex to deinitialize.
 */
void mutex_deinit(mutex_t* mtx);

/**
 * @brief Acquires a mutex, blocking until it is available.
 *
 * If the mutex is already owned by the current thread, this function will return immediately.
 *
 * @param mtx Pointer to the mutex to acquire.
 */
void mutex_acquire(mutex_t* mtx);

/**
 * @brief Acquires a mutex, blocking until it is available or the timeout is reached.
 *
 * If the mutex is already owned by the current thread, this function will return immediately.
 *
 * @param mtx Pointer to the mutex to acquire.
 * @param timeout Timeout in clock ticks or `CLOCKS_NEVER` to wait indefinitely.
 * @return true if the mutex was acquired, false if the timeout was reached.
 */
bool mutex_acquire_timeout(mutex_t* mtx, clock_t timeout);

/**
 * @brief Releases a mutex.
 *
 * The mutex must be owned by the current thread.
 *
 * @param mtx Pointer to the mutex to release.
 */
void mutex_release(mutex_t* mtx);

static inline void mutex_cleanup(mutex_t** mtx)
{
    mutex_release(*mtx);
}

/** @} */
