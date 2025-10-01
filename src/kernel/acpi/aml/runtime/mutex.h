#pragma once

#include "acpi/aml/aml_object.h"

/**
 * @brief Mutex
 * @defgroup kernel_acpi_aml_mutex Mutex
 * @ingroup kernel_acpi_aml
 *
 * This module provides functionality for acquiring and releasing AML mutexes.
 *
 * @see Section 19.6.89 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief Mutex Stack Entry
 * @struct aml_mutex_stack_entry_t
 */
typedef struct
{
    aml_object_t* mutex;
    aml_sync_level_t prevLevel;
} aml_mutex_stack_entry_t;

/**
 * @brief Mutex Stack
 * @struct aml_mutex_stack_t
 *
 * The mutex stack keeps track of the currently acquired mutexes and the current synchronization level.
 */
typedef struct
{
    aml_sync_level_t currentSyncLevel;        //!< Current synchronization level (0-15).
    uint64_t acquiredMutexCount;              //!< Number of currently acquired mutexes.
    uint64_t acquiredMutexCapacity;           //!< Capacity of the acquired mutexes stack.
    aml_mutex_stack_entry_t* acquiredMutexes; //!< Stack of acquired mutexes, used in a LIFO manner.
} aml_mutex_stack_t;

/**
 * @brief Initialize a mutex stack.
 *
 * @param mutexStack Pointer to the mutex stack to initialize.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_mutex_stack_init(aml_mutex_stack_t* mutexStack);

/**
 * @brief Deinitialize a mutex stack.
 *
 * This will release any acquired mutexes and free any allocated memory.
 *
 * @param mutexStack Pointer to the mutex stack to deinitialize.
 */
void aml_mutex_stack_deinit(aml_mutex_stack_t* mutexStack);

/**
 * @brief Acquire a mutex, blocking until it is available or the timeout is reached.
 *
 * If the mutex is already owned by the current thread, this function will return immediately.
 * If the mutex has a lower SyncLevel than the current SyncLevel, this function will fail.
 *
 * @param state Pointer to the AML state.
 * @param mutex Pointer to the mutex object to acquire.
 * @param timeout Timeout in clock ticks or `CLOCKS_NEVER` to wait indefinitely.
 * @return If the mutex was acquired, 0. If the timeout was reached, 1. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_mutex_stack_acquire(aml_mutex_stack_t* mutexStack, aml_object_t* mutex, clock_t timeout);

/**
 * @brief Release a mutex.
 *
 * The mutex must have a SyncLevel equal to the current SyncLevel and must be owned by the current thread.
 *
 * @param state Pointer to the AML state.
 * @param mutex Pointer to the mutex object to release.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_mutex_stack_release(aml_mutex_stack_t* mutexStack, aml_object_t* mutex);

/** @} */
