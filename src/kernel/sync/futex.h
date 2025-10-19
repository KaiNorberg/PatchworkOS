#pragma once

#include "fs/vfs.h"
#include "sched/wait.h"
#include "utils/map.h"

#include <sys/proc.h>

/**
 * @brief Fast User-space Mutex.
 * @defgroup kernel_sync_futex Fast User-space Mutex
 * @ingroup kernel_sync
 *
 * Patchwork uses a Futex (Fast User-space Mutex) implementation to let user space implement synchronization primitives
 * like mutexes and conditional variables efficiently.
 *
 * @{
 */

/**
 * @brief Futex structure.
 * @struct futex_t
 */
typedef struct
{
    map_entry_t entry;
    wait_queue_t queue;
} futex_t;

/**
 * @brief Per-process futex context.
 * @struct futex_ctx_t
 */
typedef struct
{
    map_t futexes;
    lock_t lock;
} futex_ctx_t;

/**
 * @brief Initialize a per-process futex context.
 *
 * @param ctx Pointer to the futex context to initialize.
 */
void futex_ctx_init(futex_ctx_t* ctx);

/**
 * @brief Deinitialize a per-process futex context.
 * *
 * @param ctx Pointer to the futex context to deinitialize.
 */
void futex_ctx_deinit(futex_ctx_t* ctx);

/** @} */
