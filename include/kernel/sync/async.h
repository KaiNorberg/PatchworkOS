#pragma once

#include <kernel/config.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/request.h>
#include <kernel/log/panic.h>

#include <string.h>
#include <sys/async.h>

/**
 * @brief Asynchronous Rings
 * @defgroup kernel_sync_rings Rings
 * @ingroup kernel_sync
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
    ASYNC_CTX_BUSY = 1 << 0,   ///< Context is currently being used.
    ASYNC_CTX_MAPPED = 1 << 1, ///< Context rings are mapped.
} async_ctx_flags_t;

/**
 * @brief The kernel-side asynchronous context structure.
 * @struct async_ctx_t
 */
typedef struct async_ctx
{
    async_rings_t rings;    ///< Asynchronous rings information.
    request_t* requests; ///< A preallocated array of requests, one for each CQE.
    list_t freeTasks;        ///< Free list of tasks.
    void* userAddr;         ///< Userspace address of the rings.
    void* kernelAddr;       ///< Kernel address of the rings.
    size_t pageAmount;      ///< Amount of pages mapped for the rings.
    space_t* space;         ///< Pointer to the owning address space.
    wait_queue_t waitQueue; ///< Wait queue for completions.
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

/**
 * @brief Acquire a async context.
 *
 * @param ctx Pointer to the context to acquire.
 * @return On success, `0`. On failure, `ERR`.
 */
static inline uint64_t async_ctx_acquire(async_ctx_t* ctx)
{
    async_ctx_flags_t expected = atomic_load(&ctx->flags);
    if (!(expected & ASYNC_CTX_BUSY) &&
        atomic_compare_exchange_strong(&ctx->flags, &expected, expected | ASYNC_CTX_BUSY))
    {
        return 0;
    }

    return ERR;
}

/**
 * @brief Release a async context.
 *
 * @param ctx Pointer to the context to release.
 */
static inline void async_ctx_release(async_ctx_t* ctx)
{
    atomic_fetch_and(&ctx->flags, ~ASYNC_CTX_BUSY);
}

/**
 * @brief Push a completion queue entry (CQE) to the completion queue.
 *
 * @param ctx Pointer to the async context.
 * @param cqe Pointer to the CQE to push.
 */
static inline void async_ctx_push_cqe(async_ctx_t* ctx, async_cqe_t* cqe)
{
    async_rings_t* rings = &ctx->rings;

    uint32_t tail = atomic_load_explicit(&rings->shared->ctail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&rings->shared->chead, memory_order_acquire);

    if ((tail - head) >= rings->centries)
    {
        /// @todo Handle overflow properly.
        panic(NULL, "Async completion queue overflow");
    }

    rings->cqueue[tail & rings->cmask] = *cqe;
    atomic_store_explicit(&rings->shared->ctail, tail + 1, memory_order_release);

    wait_unblock(&ctx->waitQueue, WAIT_ALL, EOK);
}

/** @} */