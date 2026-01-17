#pragma once

#include <kernel/config.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/request.h>

#include <string.h>
#include <sys/rings.h>

/**
 * @brief Asynchronous Rings
 * @defgroup kernel_sync_async Async
 * @ingroup kernel_sync
 *
 * @see libstd_rings for the userspace rings API.
 *
 * @{
 */

/**
 * @brief Async context flags.
 * @enum async_ctx_flags_t
 */
typedef enum
{
    ASYNC_CTX_NONE = 0,        ///< No flags set.
    ASYNC_CTX_BUSY = 1 << 0,   ///< Context is currently being used, used for fast locking.
    ASYNC_CTX_MAPPED = 1 << 1, ///< Context rings are mapped.
} async_ctx_flags_t;

/**
 * @brief The kernel-side asynchronous context structure.
 * @struct async_ctx_t
 */
typedef struct async_ctx
{
    rings_t rings;            ///< Asynchronous rings information.
    request_pool_t* requests; ///< Pool of preallocated requests.
    void* userAddr;           ///< Userspace address of the rings.
    void* kernelAddr;         ///< Kernel address of the rings.
    size_t pageAmount;        ///< Amount of pages mapped for the rings.
    space_t* space;           ///< Pointer to the owning address space.
    wait_queue_t waitQueue;   ///< Wait queue for completions.
    process_t* process;       ///< Holds a reference to the owner process while there are pending requests.
    _Atomic(async_ctx_flags_t) flags;
} async_ctx_t;

/**
 * @brief Initialize a async context.
 *
 * @param ctx Pointer to the context to initialize.
 */
void async_ctx_init(async_ctx_t* ctx);

/**
 * @brief Deinitialize a async context.
 *
 * @param ctx Pointer to the context to deinitialize.
 */
void async_ctx_deinit(async_ctx_t* ctx);

/**
 * @brief Notify the context of new SQEs.
 *
 * @param ctx Pointer to the context.
 * @param amount The number of SQEs to process.
 * @param wait The minimum number of CQEs to wait for.
 * @return On success, the number of SQEs processed. On failure, `ERR` and `errno` is set.
 */
uint64_t async_ctx_notify(async_ctx_t* ctx, size_t amount, size_t wait);

/** @} */