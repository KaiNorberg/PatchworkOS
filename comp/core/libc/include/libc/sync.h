#ifndef _SYS_SYNC_H
#define _SYS_SYNC_H 1

#include <libc/syscall.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libc/NULL.h"
#include "_libc/PAGE_SIZE.h"
#include "_libc/clock_t.h"
#include "_libc/config.h"

/**
 * @brief Synchronization control.
 * @ingroup libc
 * @defgroup libc_sync Sync Control
 *
 * The `sys/sync.h` header provides functions for managing kernel-backed synchronization objects.
 *
 * @{
 */

/**
 * @brief Sync control operation enum.
 * @enum sync_ctl_t
 */
typedef enum
{
    /**
     * @brief Wait until the timeout expires or the value changes.
     *
     * If the value at the address is not equal to `val`, the call returns immediately.
     *
     * Otherwise, the calling thread is put to sleep until another thread wakes it up or the specified timeout expires.
     */
    SYNC_WAIT,
    /**
     * @brief Wake up one or more threads waiting on the primitive.
     *
     * Wakes up a maximum of `val` number of threads that are currently waiting on the primitive at the specified
     * address.
     */
    SYNC_WAKE,
} sync_op_t;

/**
 * @brief System call for performing
 *
 * @param addr A pointer to an atomic 64-bit unsigned integer.
 * @param val The value used by the operation, its meaning depends on the operation.
 * @param op The operation to perform (e.g., `SYNC_WAIT` or `SYNC_WAKE`).
 * @param timeout An optional timeout for `SYNC_WAIT`. If `CLOCKS_NEVER`, it waits forever.
 * @param result Output pointer for the result, can be `NULL`.
 * @return An appropriate status value.
 */
static inline status_t sync_ctl(atomic_uint64_t* addr, uint64_t val, sync_op_t op, clock_t timeout, uint64_t* result)
{
    return syscall4(SYS_SYNC_CTL, result, (uintptr_t)addr, val, op, timeout);
}

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
