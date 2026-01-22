#pragma once

#include <kernel/fs/path.h>
#include <kernel/mem/mdl.h>
#include <kernel/mem/pool.h>
#include <kernel/sync/lock.h>

#include <assert.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/fs.h>
#include <sys/ioring.h>
#include <sys/list.h>
#include <sys/math.h>
#include <time.h>

typedef struct file file_t;
typedef struct process process_t;

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
 * The IRP system is designed to allow multiple subsystems or functions to asynchronously "call" each other.
 *
 * Consider a traditional synchronous set of functions:
 *
 * ```
 * int fun_b(int val)
 * {
 *     wait_until_data_ready();
 *     return val * get_data();
 * }
 *
 * int fun_a(int val)
 * {
 *     return fun_b(val) * 2;
 * }
 *
 * int result = fun_a(5);
 * // Do stuff with the result
 * ```
 *
 * The above may seem obvious, but in a asynchronous kernel we are not allowed to block, as such `fun_b()` should not be
 * implemented this way. We must however still be able to achieve the same result.
 *
 * @note In practice its possible that more than just one layer needs to block, as such the IRP system needs to handle
 * such cases as well.
 *
 * The idea behind IRPs is to effectively create a call stack which is detached from the actual CPU stack. Each frame in
 * the IRP stack represents a function call with associated arguments and a "return address" in the form of a function
 * pointer and "local variables" in the form of a context pointer.
 *
 * Using the IRP system, the above code would be written as:
 *
 * ```
 * void fun_b_interrupt(void)
 * {
 *     irp_t* irp = pop_irp_from_list();
 *     irp_frame_t* frame = irp_current(irp);
 *     irp->res.u64 = frame->args[0] * get_data();
 *     irp_complete(irp);
 * }
 *
 * void fun_b(irp_t* irp)
 * {
 *     irp_frame_t* frame = irp_current(irp);
 *     if (can_complete_now())
 *     {
 *         irp->res.u64 = frame->args[0] * get_data();
 *         irp_complete(irp);
 *         return;
 *     }
 *
 *     add_irp_to_list(irp);
 * }
 *
 * void fun_a_return_address(irp_t* irp, void* ctx)
 * {
 *     irp->res.u64 *= 2;
 *     irp_complete(irp);
 *     return;
 * }
 *
 * void fun_a(irp_t* irp)
 * {
 *     irp_frame_t* current = irp_current(irp);
 *     irp_frame_t* next = irp_next(irp);
 *
 *     next->args[0] = current->args[0];
 *     next->ret = fun_a_return_address;
 *
 *     irp_call(irp, fun_b);
 * }
 *
 * void my_return_address(irp_t* irp, void* ctx)
 * {
 *     //  Do stuff with the result in irp->result.
 *     irp_free(irp);
 * }
 *
 * irp_t* irp = irp_new(pool);
 *
 * // Setup the "stack frame" for our function call.
 * irp_frame_t* next = irp_next(irp);
 * next->args[0] = 5;
 * next->ret = my_return_address;
 *
 * // Call `fun_a()` and advance the stack frame.
 * irp_call(irp, fun_a);
 *
 * // Continue executing even if fun_b() cannot complete immediately.
 * ```
 *
 * ## Cancellation
 *
 * The current owner of a IRP is responsible for handling cancellation. The current owner being the last subsystem to
 * advance the IRP stack frame.
 *
 * @note Intuitively, we can think of "cancelling" a IRP to be equivalent to causing the last completion to fail, thus
 * resulting in all the other return addresses to fail as well. In the examples from the Completion section, it would be
 * as though the synchronous `fun_c()` returned an error code instead of the data.
 *
 * The owner implements cancellation by calling `irp_set_cancel()` to set a cancellation callback. When an IRP is to be
 * cancelled or timed out the cancellation callback will be invoked and atomically exchanged with a `IRP_CANCELLED`
 * sentinel value. At which point the owner should cleanup the IRP and call `irp_complete(irp)`.
 *
 * It is not possible for the IRP system to perform this atomic exchange for return addresses. As such, to avoid race
 * conditions while completing an IRP, it is vital that the owner of the IRP atomically exchanges the cancellation
 * callback with the `IRP_CANCELLED` sentinel value. For the sake of convenience, the `irp_claim()` function is provided
 * to perform this operation.
 *
 * Below is an example of how to safely implement a return address with an associated cancellation callback:
 *
 * ```
 * void my_return_address(irp_t* irp, void* ctx)
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

/**
 * @brief IRP return address type.
 *
 * @param irp Pointer to the IRP.
 * @param ctx Context pointer.
 */
typedef void (*irp_ret_t)(irp_t* irp, void* ctx);

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

#define IRP_ARGS_MAX 5 ///< The maximum number of 64-bit arguments in a IRP frame.

/**
 * @brief IRP stack frame structure.
 * @struct irp_frame_t
 */
typedef struct irp_frame
{
    irp_ret_t ret; ///< Return Address.
    void* local; ///< Local context.
    union {
        struct
        {
            file_t* file;
            mdl_t* buffer;
            size_t len;
            ssize_t off;
        } read;
        uint64_t args[IRP_ARGS_MAX];
        sqe_args_t sqe;
    };
} irp_frame_t;

#define IRP_FRAME_MAX 5 ///< The maximum number of frames in a IRP stack.

/**
 * @brief I/O Request Packet structure.
 * @struct irp_t
 *
 * The I/O Request Packet structure is designed to preallocate as much as possible such that in the common case there is
 * no need for any allocation beyond the allocation of the IRP itself. This does require careful consideration of
 * padding, alignment and field sizes to keep it within a reasonable size.
 *
 * @todo Consider raising `IRP_FRAME_MAX` if needed, it will add more cache lines tho.
 *
 * @see kernel_io for more information for each possible verb.
 */
typedef struct ALIGNED(64) irp
{
    list_entry_t entry;           ///< Used to store the IRP in various lists.
    list_entry_t timeoutEntry;    ///< Used to store the IRP in the timeout queue.
    _Atomic(irp_cancel_t) cancel; ///< Cancellation callback, must be atomic to ensure an IRP is only cancelled once.
    union {
        clock_t timeout;  ///< The timeout starting from when the IRP is added to a timeout queue.
        clock_t deadline; ///< The time at which the IRP will be removed from a timeout queue.
    };
    void* data; ///< Private data for the operation, will be returned in the completion entry.
    union {
        uint64_t u64;
        int64_t s64;
        size_t read;
    } res;
    mdl_t mdl;                        ///< A preallocated memory descriptor list for use by the IRP.
    pool_idx_t index;                 ///< Index of the IRP in its pool.
    pool_idx_t next;                  ///< Index of the next IRP in a chain or in the free list.
    cpu_id_t cpu;                     ///< The CPU whose timeout queue the IRP is in.
    uint8_t err;                      ///< The error code of the operation, also used to specify its current state.
    uint8_t frame;                    ///< The index of the current frame in the stack.
    irp_frame_t stack[IRP_FRAME_MAX]; ///< The frame stack, grows downwards.
} irp_t;

static_assert(sizeof(irp_t) == 448, "irp_t is not 448 bytes");

/**
 * @brief Request pool structure.
 * @struct irp_pool
 */
typedef struct irp_pool
{
    void* ctx;
    process_t* process; ///< Will only hold a reference if there is at least one allocated IRP.
    pool_t pool;
    irp_t irps[];
} irp_pool_t;

/**
 * @brief Allocate a new IRP pool.
 *
 * @param size The amount of requests to allocate.
 * @param process The process that will own the IRPs allocated from this pool.
 * @param ctx The context of the IRP pool.
 * @return On success, a pointer to the new IRP pool. On failure, `NULL` and `errno` is set.
 */
irp_pool_t* irp_pool_new(size_t size, process_t* process, void* ctx);

/**
 * @brief Free a IRP pool.
 *
 * @param pool Pointer to the IRP pool to free.
 */
void irp_pool_free(irp_pool_t* pool);

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
 * The pool that the IRP was allocated from, and its context, can be retrieved using the `irp_get_pool()`
 * function.
 *
 * @param pool Pointer to the IRP pool.
 * @return On success, a pointer to the allocated IRP. On failure, `NULL` and `errno` is set.
 */
irp_t* irp_new(irp_pool_t* pool);

/**
 * @brief Free a IRP back to its pool.
 *
 * @param irp Pointer to the IRP to free.
 */
void irp_free(irp_t* irp);

/**
 * @brief Retrieve the IRP pool that an IRP was allocated from.
 *
 * @param irp Pointer to the IRP.
 * @return Pointer to the IRP pool.
 */
static inline irp_pool_t* irp_get_pool(irp_t* irp)
{
    return CONTAINER_OF(irp, irp_pool_t, irps[irp->index]);
}

/**
 * @brief Retrieve the context of the IRP pool that an IRP was allocated from.
 *
 * @param irp Pointer to the IRP.
 * @return Pointer to the context.
 */
static inline void* irp_get_ctx(irp_t* irp)
{
    return irp_get_pool(irp)->ctx;
}

/**
 * @brief Retrieve the process that owns an IRP.
 *
 * @param irp Pointer to the IRP.
 * @return Pointer to the process.
 */
static inline process_t* irp_get_process(irp_t* irp)
{
    return irp_get_pool(irp)->process;
}

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
 * @return `true` if the IRP was successfully claimed, `false` if it was already cancelled.
 */
static inline bool irp_claim(irp_t* irp)
{
    return irp_set_cancel(irp, NULL) != IRP_CANCELLED;
}

/**
 * @brief Retrieve the next IRP in a chain and clear the next field.
 *
 * @param irp Pointer to the current IRP.
 * @return Pointer to the next IRP, or `NULL` if there is no next IRP.
 */
static inline irp_t* irp_chain_next(irp_t* irp)
{
    irp_pool_t* pool = irp_get_pool(irp);
    if (irp->next == POOL_IDX_MAX)
    {
        return NULL;
    }

    irp_t* next = &pool->irps[irp->next];
    irp->next = POOL_IDX_MAX;
    return next;
}

/**
 * @brief Retrieve the current frame in the IRP stack.
 *
 * @param irp Pointer to the IRP to retrieve the frame from.
 * @return Pointer to the current frame.
 */
static inline irp_frame_t* irp_current(irp_t* irp)
{
    assert(irp->frame < IRP_FRAME_MAX);
    return &irp->stack[irp->frame];
}

/**
 * @brief Retrieve the next frame in the IRP stack.
 *
 * @param irp Pointer to the IRP to retrieve the frame from.
 * @return Pointer to the next frame, or `NULL` if we are at the bottom of the stack.
 */
static inline irp_frame_t* irp_next(irp_t* irp)
{
    if (irp->frame == 0)
    {
        return NULL;
    }
    return &irp->stack[irp->frame - 1];
}
/**
 * @brief Call a function with an IRP, advancing the frame in the IRP stack.
 * 
 * @param irp Pointer to the IRP.
 * @param func The function to call.
 */
static inline void irp_call(irp_t* irp, void (*func)(irp_t* irp))
{
    assert(irp->frame > 0);
    irp->frame--;
    func(irp);
}

/**
 * @brief Complete the current frame in the IRP stack.
 *
 * @param irp Pointer to the IRP to complete.
 */
static inline void irp_complete(irp_t* irp)
{
    if (irp->frame == IRP_FRAME_MAX)
    {
        return;
    }

    irp_frame_t* loc = irp_current(irp);
    irp->frame++;

    if (irp->frame == IRP_FRAME_MAX)
    {
        irp_timeout_remove(irp);
    }

    loc->ret(irp, loc->local);
}

/**
 * @brief Helper to set an error code and complete the IRP.
 * 
 * @param irp Pointer to the IRP.
 * @param err The error code to set.
 */
static inline void irp_error(irp_t* irp, uint8_t err)
{
    irp->err = err;
    irp_complete(irp);
}

/**
 * @brief Attempt to cancel an IRP.
 *
 * @param irp Pointer to the IRP to cancel.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t irp_cancel(irp_t* irp);

/** @} */