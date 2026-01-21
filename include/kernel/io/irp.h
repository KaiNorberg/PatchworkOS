#pragma once

#include <kernel/fs/path.h>
#include <kernel/mem/mem_desc.h>
#include <kernel/mem/pool.h>
#include <kernel/sync/lock.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/ioring.h>
#include <time.h>

typedef struct irp irp_t;

/**
 * @brief I/O Request Packet.
 * @defgroup kernel_io_irp I/O Request Packet
 * @ingroup kernel_io
 *
 * The I/O Request Packet is a lock-less, self-contained, layered, completion-based request that act as the primary
 * structure used internally by the kernel for asynchronous operations.
 *
 * The IRP system is designed to be generic enough to be used by any system in the kernel, however it is primarily used
 * by the ring system.
 *
 * @warning While the cancellation or completion of an IRP is thread safe, the setup of an IRP is not (as in pushing
 * layers to it). As such, its up to the caller to ensure that only one thread is manipulating it during setup.
 *
 * ## Completion
 *
 * The IRP system is designed around the concept of layered completions as it may take more than one subsystem within
 * the kernel to complete a IRP.
 * the kernel to complete an IRP.
 *
 * Consider a traditional synchronous set of functions:
 *
 * ```
 * int fun_c(void)
 * {
 *     wait_until_data_ready();
 *     return data;
 * }
 *
 * int fun_b(int val)
 * {
 *     return fun_c(val) + 1;
 * }
 *
 * int fun_a(int val)
 * {
 *     return fun_b(val) * 2;
 * }
 *
 * int result = fun_a();
 * // Do stuff with the result
 * ```
 *
 * When the code is executed, `fun_a()` would be called, which calls `fun_b()`, which in turn calls `fun_c()`. At this
 * point `fun_c()` will block, causing the scheduler to switch to another thread until the data is ready. Once the data
 * is ready, `fun_c()` will "complete" and return, followed by `fun_b()` and finally `fun_a()`, with the final result
 * being stored in `result`.
 *
 * The above may seem obvious, but in a asynchronous kernel we are not allowed to block but must still be able to
 * achieve the same result. As such, we need a way of representing the layered calls and their completions.
 *
 * @note In practice its possible that more than just one layer needs to block, as such the IRP system needs to handle
 * such cases as well.
 *
 * Using the IRP system, the above code would be written as:
 *
 * ```
 * void fun_c_complete(irp_t* irp, void* ctx)
 * {
 *     irp->result = get_data();
 *     irp_complete(irp);
 * }
 *
 * void fun_b_complete(irp_t* irp, void* ctx)
 * {
 *     irp->result += 1;
 *     irp_complete(irp);
 * }
 *
 * void fun_a_complete(irp_t* irp, void* ctx)
 * {
 *     irp->result *= 2;
 *     irp_complete(irp);
 * }
 *
 * void fun_c(irp_t* irp)
 * {
 *     if (can_complete_now())
 *     {
 *         irp->result = get_data();
 *         irp_complete(irp);
 *     }
 *     else
 *     {
 *         irp_push(irp, fun_c_complete, NULL);
 *     }
 * }
 *
 * void fun_b(irp_t* irp)
 * {
 *     irp_push(irp, fun_b_complete, NULL);
 *     fun_c(irp);
 * }
 *
 * void fun_a(irp_t* irp)
 * {
 *     irp_push(irp, fun_a_complete, NULL);
 *     fun_b(irp);
 * }
 *
 * void my_completion(irp_t* irp)
 * {
 *     //  Do stuff with the result in irp->result.
 *     irp_free(irp);
 * }
 *
 * irp_t* irp = irp_new(pool, NULL);
 * // We can set arguments here if we want.
 * irp_push(irp, my_completion, NULL); // Our completion to handle cleanup.
 * fun_a_do(irp);
 * // Continue executing even if fun_c() cannot complete immediately.
 * ```
 *
 * When `fun_a()` is called, it pushes its completion onto the IRP stack, followed by `fun_b()` pushing its completion,
 * and finally `fun_c()` which may either complete immediately or push its completion if it cannot complete right away.
 *
 * Each time a completion is called via `irp_complete()`, the next completion on the stack is called until the stack is
 * empty, at which point the IRP is considered fully completed.
 *
 * A real world example of this would be the ring system allocating an IRP, pushing a completion which will add a
 * `cqe_t` to its rings, before passing the IRP to the VFS which may pass it to a filesystem. Each layer pushing its own
 * completion to handle its part of the operation.
 *
 * Finally, it is also possible to use the `irp_dispatch()` function. This function allows us to dispatch the IRP to a
 * appropriate handler depending on the IRPs specified verb. For example:
 *
 * ```
 * void my_completion(irp_t* irp)
 * {
 *     //  Do stuff with the result in irp->result.
 *     irp_free(irp);
 * }
 *
 * irp_t* irp = irp_new(pool, NULL);
 *
 * // Set our desired verb and arguments.
 * irp->verb = VERB_OPEN;
 * irp->open.from = from;
 * irp->open.path = path;
 * irp->open.length = strlen(path);
 *
 * // Our completion to receive the result.
 * irp_push(irp, my_completion, NULL);
 *
 * // Finally, dispatch the IRP to the appropriate handler.
 * irp_dispatch(irp);
 * // Continue executing even if the operation cannot complete immediately.
 * ```
 *
 * ## Cancellation
 *
 * The current owner of a IRP is responsible for handling cancellation. The current owner being the last subsystem to
 * push a completion onto the IRP stack.
 *
 * @note Intuitively, we can think of "cancelling" a IRP to be equivalent to causing the last completion to fail, thus
 * resulting in all the other completions to fail as well. In the examples from the Completion section, it would be as
 * though the synchronous `fun_c()` returned an error code instead of the data.
 *
 * The owner implements cancellation by calling `irp_set_cancel()` to set a cancellation callback when it pushes its
 * completion. When an IRP is to be cancelled or timed out the cancellation callback will be invoked and atomically
 * exchanged with a `IRP_CANCELLED` sentinel value. At which point the owner should cleanup the IRP and call
 * `irp_complete()`.
 *
 * It is not possible for the IRP system to perform this atomic exchange for completions. As such, to avoid race
 * conditions while completing an IRP, it is vital that the owner of the IRP atomically exchanges the cancellation
 * callback with the `IRP_CANCELLED` sentinel value. For the sake of convenience, the `irp_claim()` function is provided
 * to perform this operation.
 *
 * Below is an example of how to safely implement a completion with an associated cancellation callback:
 *
 * ```
 * void my_completion(irp_t* irp, void* ctx)
 * {
 *     if (!irp_claim(irp))
 *     {
 *         // The IRP has already been cancelled, nothing to do here.
 *         return;
 *     }
 *
 *     // Complete the IRP...
 *
 *     irp_complete(irp);
 * }
 *
 * uint64_t my_cancel(irp_t* irp)
 * {
 *     // Cancellation callback is automatically cleared.
 *
 *     if (irp->err == ETIMEDOUT)
 *     {
 *         // We timed out.
 *     }
 *     if (irp->err == ECANCELED)
 *     {
 *         // We were explicitly cancelled.
 *     }
 *
 *     // Perform cancellation...
 *
 *     uint64_t result = ...;
 *     if (result == ERR) // If an error occurs we can reassign the cancellation callback.
 *     {
 *         irp_set_cancel(irp, my_cancel);
 *         return ERR;
 *     }
 *
 *     irp_complete(irp);
 *     return 0;
 * }
 * ```
 *
 * ## Error Values
 *
 * The IRP system uses the `err` field to indicate both the current state of the IRP as well as any error that may have
 * occurred during its processing.
 *
 * Included below are a list of "special" values which the IRP system will recognize:
 *
 * - `EOK`: Operation completed successfully.
 * - `ECANCELED`: Operation was cancelled.
 * - `ETIMEDOUT`: Operation timed out.
 * - `EINPROGRESS`: Operation is in a timeout queue.
 *
 * @see kernel_io for the ring system.
 * @see [Wikipedia](https://en.wikipedia.org/wiki/I/O_request_packet) for more information about IRPs.
 * @see [Microsoft _IRP](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_irp) for information
 * on how Windows NT implements IRPs.
 * @{
 */

#define IRP_LOC_MAX 8 ///< The maximum number of locations in a IRP.

#define IRP_ARGS_MAX SQE_MAX_ARGS ///< The maximum number of arguments in an IRP.

/**
 * @brief IRP completion callback type.
 *
 * @param irp Pointer to the IRP.
 * @param ctx Context pointer.
 */
typedef void (*irp_complete_t)(irp_t* irp, void* ctx);

/**
 * @brief IRP cancellation callback type.
 *
 * @param irp Pointer to the IRP.
 * @return On success, `0`. On failure, `ERR`.
 */
typedef uint64_t (*irp_cancel_t)(irp_t* irp);

/**
 * @brief Sentinel value indicating that the IRP has been cancelled.
 */
#define IRP_CANCELLED ((irp_cancel_t)1)

/**
 * @brief IRP location structure.
 * @struct irp_loc
 */
typedef struct irp_loc
{
    void* ctx;
    irp_complete_t complete;
} irp_loc_t;

/**
 * @brief I/O Request Packet structure.
 * @struct irp_t
 *
 * @note We need the ability to store both the original arguments from a SQE and the parsed arguments. For example,
 * opening a `fd_t` into a `file_t*`. As such, to avoid using another cache line, the SQE is stored in a union with the
 * parsed arguments.
 */
typedef struct ALIGNED(64) irp
{
    list_entry_t entry;           ///< Used to store the IRP in various lists.
    list_entry_t timeoutEntry;    ///< Used to store the IRP in the timeout queue.
    _Atomic(irp_cancel_t) cancel; ///< Cancellation callback, must be atomic to ensure an IRP is only cancelled once.
    union {
        struct
        {
            verb_t verb;       ///< Verb specifying the action to perform.
            sqe_flags_t flags; ///< Submission flags.
            union {
                clock_t timeout;  ///< The timeout starting from when the IRP is added to a timeout queue.
                clock_t deadline; ///< The time at which the IRP will be removed from a timeout queue.
            };
            void* data; ///< Private data for the operation, will be returned in the completion entry.
            union {
                struct
                {
                    file_t* from;
                    mem_desc_t* path;
                } open;
                uint64_t _args[IRP_ARGS_MAX];
            };
        };
        sqe_t sqe; ///< The original SQE for this IRP.
    };
    uint64_t result;  ///< Result of the IRP.
    errno_t err;      ///< The error code of the operation, also used to specify its current state.
    pool_idx_t index; ///< Index of the IRP in its pool.
    pool_idx_t next;  ///< Index of the next IRP in a chain or in the free list.
    cpu_id_t cpu;     ///< The CPU whose timeout queue the IRP is in.
    uint8_t location; ///< The index of the current location in the stack.
    uint8_t _reserved2[5];
    irp_loc_t stack[IRP_LOC_MAX]; ///< The location stack, grows downwards.
} irp_t;

static_assert(offsetof(irp_t, verb) == offsetof(irp_t, sqe.verb), "verb offset mismatch");
static_assert(offsetof(irp_t, flags) == offsetof(irp_t, sqe.flags), "flags offset mismatch");
static_assert(offsetof(irp_t, timeout) == offsetof(irp_t, sqe.timeout), "timeout offset mismatch");
static_assert(offsetof(irp_t, data) == offsetof(irp_t, sqe.data), "data offset mismatch");
static_assert(offsetof(irp_t, _args) == offsetof(irp_t, sqe._args), "args offset mismatch");

/**
 * @brief Request pool structure.
 * @struct irp_pool
 */
typedef struct irp_pool
{
    void* ctx;
    pool_t pool;
    irp_t irps[];
} irp_pool_t;

/**
 * @brief Allocate a new IRP pool.
 *
 * @param size The amount of requests to allocate.
 * @param ctx The context of the IRP pool.
 * @return On success, a pointer to the new IRP pool. On failure, `NULL` and `errno` is set.
 */
irp_pool_t* irp_pool_new(size_t size, void* ctx);

/**
 * @brief Free a IRP pool.
 *
 * @param pool Pointer to the IRP pool to free.
 */
void irp_pool_free(irp_pool_t* pool);

/**
 * @brief Retrieve the IRP pool that an IRP was allocated from.
 *
 * @param irp Pointer to the IRP.
 * @return Pointer to the IRP pool.
 */
static inline irp_pool_t* irp_pool_get(irp_t* irp)
{
    return CONTAINER_OF(irp, irp_pool_t, irps[irp->index]);
}

/**
 * @brief Add an IRP to a per-CPU timeout queue with the timeout specified in the IRP.
 *
 * @param irp Pointer to the IRP to add.
 */
void irp_timeout_add(irp_t* irp);

/**
 * @brief Remove an IRP from its per-CPU timeout queue.
 *
 * @param irp Pointer to the IRP to remove.
 */
void irp_timeout_remove(irp_t* irp);

/**
 * @brief Check and handle expired IRP timeouts on the current CPU.
 */
void irp_timeouts_check(void);

/**
 * @brief Allocate a new IRP from a pool.
 *
 * The pool that the IRP was allocated from, and its context, can be retrieved using the `irp_pool_get()`
 * function.
 *
 * @note If a SQE is provided then the IRP will be considered a user IRP, causing the `irp_handler_t::enter` and
 * `irp_handler_t::leave` callbacks to be invoked on the IRP when its dispatched and freed respectively. Otherwise, the
 * caller is responsible for the lifecycle and arguments of the IRP.
 *
 * @param pool Pointer to the IRP pool.
 * @param sqe The Submission Queue Entry associated with the IRP, if `NULL` the IRP will be a kernel IRP.
 * @return On success, a pointer to the allocated IRP. On failure, `NULL` and `errno` is set.
 */
irp_t* irp_new(irp_pool_t* pool, sqe_t* sqe);

/**
 * @brief Free a IRP back to its pool.
 *
 * If the IRP is a user IRP, the `irp_handler_t::leave` callback will be invoked before freeing the IRP.
 *
 * @param irp Pointer to the IRP to free.
 */
void irp_free(irp_t* irp);

/**
 * @brief Set the cancellation callback for an IRP.
 *
 * @param irp Pointer to the IRP.
 * @param cancel The cancellation callback.
 * @return The previous cancellation callback.
 */
static inline irp_cancel_t irp_set_cancel(irp_t* irp, irp_cancel_t cancel)
{
    irp_cancel_t expected = atomic_load(&irp->cancel);
    while (expected != IRP_CANCELLED)
    {
        if (atomic_compare_exchange_weak(&irp->cancel, &expected, cancel))
        {
            return expected;
        }
    }
    return IRP_CANCELLED;
}

/**
 * @brief Attempt to claim an IRP for completion.
 *
 * @param irp Pointer to the IRP.
 * @return `true` if the IRP was successfully claimed, `false` if it was already cancelled or claimed.
 */
static inline bool irp_claim(irp_t* irp)
{
    return irp_set_cancel(irp, NULL) != IRP_CANCELLED;
}

/**
 * @brief Retrieve the context of the IRP pool that an IRP was allocated from.
 *
 * @param irp Pointer to the IRP.
 * @return Pointer to the context.
 */
static inline void* irp_get_ctx(irp_t* irp)
{
    return irp_pool_get(irp)->ctx;
}

/**
 * @brief Retrieve the next IRP and clear the next field.
 *
 * @param irp Pointer to the current IRP.
 * @return Pointer to the next IRP, or `NULL` if there is no next IRP.
 */
static inline irp_t* irp_next(irp_t* irp)
{
    irp_pool_t* pool = irp_pool_get(irp);
    if (irp->next == POOL_IDX_MAX)
    {
        return NULL;
    }

    irp_t* next = &pool->irps[irp->next];
    irp->next = POOL_IDX_MAX;
    return next;
}

/**
 * @brief Retrieve the current location in the IRP stack.
 *
 * @param irp Pointer to the IRP to retrieve the location from.
 * @return Pointer to the current location.
 */
static inline irp_loc_t* irp_current(irp_t* irp)
{
    assert(irp->location <= IRP_LOC_MAX);
    return &irp->stack[irp->location];
}

/**
 * @brief Retrieve the next location in the IRP stack.
 *
 * @param irp Pointer to the IRP to retrieve the location from.
 * @return Pointer to the next location, or `NULL` if we are at the bottom of the stack.
 */
static inline irp_loc_t* irp_next_loc(irp_t* irp)
{
    if (irp->location == 0)
    {
        return NULL;
    }
    return &irp->stack[irp->location - 1];
}

/**
 * @brief Push a new location onto the IRP stack.
 *
 * @param irp Pointer to the IRP to push to.
 * @param complete The completion callback.
 * @param ctx The context pointer.
 */
static inline void irp_push(irp_t* irp, irp_complete_t complete, void* ctx)
{
    assert(irp->location > 0);
    assert(complete != NULL);
    irp_loc_t* loc = &irp->stack[irp->location - 1];
    loc->complete = complete;
    loc->ctx = ctx;
    irp->location--;
}

/**
 * @brief Complete the current location in the IRP stack.
 *
 * @param irp Pointer to the IRP to complete.
 */
static inline void irp_complete(irp_t* irp)
{
    if (irp->location == IRP_LOC_MAX)
    {
        return;
    }

    irp_loc_t* loc = irp_current(irp);
    irp->location++;

    if (irp->location == IRP_LOC_MAX)
    {
        irp_timeout_remove(irp);
    }

    loc->complete(irp, loc->ctx);
}

/**
 * @brief Attempt to cancel an IRP.
 *
 * @param irp Pointer to the IRP to cancel.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t irp_cancel(irp_t* irp);

/**
 * @brief Dispatch an IRP to the appropriate handler.
 *
 * If `irp->err != EINPROGRESS` the IRP is immediately completed.
 *
 * If the IRP is a user IRP and it has not yet been entered, the `irp_handler_t::enter` callback for the verb is
 * invoked.
 *
 * @param irp Pointer to the IRP to dispatch.
 */
void irp_dispatch(irp_t* irp);

/**
 * @brief Sort and validate the IRP handlers table.
 */
void irp_table_init(void);

/**
 * @brief IRP handler structure.
 * @struct irp_handler_t
 */
typedef struct
{
    verb_t verb;
    void (*enter)(irp_t* irp);   ///< Will be called on user IRPs to process arguments.
    void (*leave)(irp_t* irp);   ///< Will be called on user IRPs to cleanup resources.
    void (*handler)(irp_t* irp); ///< The handler function for the verb.
} irp_handler_t;

/**
 * @brief Linker defined start of the IRP handlers table.
 *
 * After `irp_table_init()` has sorted the IRP table, the table can be indexed by verb.
 */
extern irp_handler_t _irp_table_start[];

/**
 * @brief Linker defined end of the IRP handlers table.
 */
extern irp_handler_t _irp_table_end[];

/**
 * @brief Macro to register a IRP handler to a verb using the `._irp_table` section.
 *
 * @param _verb The verb to register the handler for.
 * @param _enter The enter function.
 * @param _leave The leave function.
 * @param _handler The handler function.
 */
#define IRP_REGISTER(_verb, _enter, _leave, _handler) \
    static irp_handler_t __irp_##_verb __attribute__((section("._irp_table"), used)) = { \
        .verb = (_verb), \
        .enter = (_enter), \
        .leave = (_leave), \
        .handler = (_handler), \
    };

/**
 * @brief Function to asynchronously do nothing.
 *
 * Usefull as a sleep or delay operation.
 *
 * @param irp Pointer to a IRP to do nothing with.
 */
void nop_do(irp_t* irp);

/** @} */