#pragma once

#include <kernel/sync/mutex.h>
#include <kernel/acpi/aml/encoding/named.h>

typedef struct aml_thread aml_thread_t;

/**
 * @brief Mutex
 * @defgroup kernel_acpi_aml_mutex Mutex
 * @ingroup kernel_acpi_aml
 *
 * This module provides functionality for acquiring and releasing AML mutexes.
 *
 * Note that mutexes currently do... nothing. Instead we just use one big mutex for the entire parser, but we
 * still implement their behaviour to check for invalid code or errors. This really shouldent matter as AML should only
 * be getting executed by one thread at a time anyway.
 *
 * @see Section 19.6.89 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief Mutex id.
 * @typedef aml_mutex_id_t
 */
typedef uint64_t aml_mutex_id_t;

/**
 * @brief Create a new mutex and return its id.
 *
 * @param mutex The mutex id to initialize.
 */
void aml_mutex_id_init(aml_mutex_id_t* mutex);

/**
 * @brief Destroy the mutex with the given id.
 *
 * @param mutex The mutex id to destroy.
 */
void aml_mutex_id_deinit(aml_mutex_id_t* mutex);

/**
 * @brief Acquire a mutex, blocking until it is available or the timeout is reached.
 *
 * If the mutex is already owned by the current thread, this function will return immediately.
 * If the mutex has a lower SyncLevel than the current SyncLevel, this function will fail.
 *
 * @param mutex The mutex to acquire.
 * @param syncLevel The SyncLevel at which to acquire the mutex.
 * @param timeout The timeout in clock ticks to wait for the mutex, or `CLOCKS_NEVER` to wait indefinitely.
 * @return On success, `0`. If timed out, 1. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_mutex_acquire(aml_mutex_id_t* mutex, aml_sync_level_t syncLevel, clock_t timeout);

/**
 * @brief Release a mutex.
 *
 * The mutex must have a SyncLevel equal to the current SyncLevel and must be owned by the current thread.
 *
 * @param mutex The mutex to release.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_mutex_release(aml_mutex_id_t* mutex);

/** @} */
