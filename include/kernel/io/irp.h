#pragma once

#include <kernel/fs/path.h>
#include <kernel/mem/mdl.h>
#include <kernel/mem/pool.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/ref.h>

#include <assert.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/fs.h>
#include <sys/ioring.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/status.h>
#include <time.h>

typedef struct file file_t;
typedef struct process process_t;
typedef struct vnode vnode_t;

typedef struct irp irp_t;

/**
 * @brief I/O Request Packet.
 * @defgroup kernel_io_irp I/O Request Packet
 * @ingroup kernel_io
 *
 * The I/O Request Packet (IRP) is a lock-less, self-contained, layered, continuation-passing request that acts as the
 * primary primitive used by the kernel for asynchronous operations.
 *
 * The IRP is designed to be generic enough to be used by any system in the kernel, however it is primarily used by the
 * I/O ring system.
 *
 * @warning While the cancellation or completion of an IRP is thread safe, the setup of an IRP is not (as in pushing
 * layers to it). It is assumed that only one thread is manipulating an IRP during its setup.
 *
 * ## Completion
 *
 * @todo Write the IRP documentation.
 *
 * ## Cancellation
 *
 * @todo Update IRP documentation for the new status system.
 * 
 * Cancelling an IRP can intuitively be considered equivalent to forcing the last completion to fail, thus resulting in
 * all the other completions to fail as well.
 *
 * The current owner of a IRP is responsible for handling cancellation by specifying a cancellation callback via
 * `irp_set_cancel()`. The current owner being the last target of a `irp_call()` or `irp_call_direct()`.
 *
 * When an IRP is cancelled or timed out the cancellation callback will be invoked and atomically exchanged with a
 * `IRP_CANCELLED` sentinel value. At which point the owner should perform whatever logic is needed to cancel the IRP,
 * if it is not possible immediately cancel the IRP it should return `PFAIL`.
 *
 * Below is an example of how to implement a completion with an associated cancellation callback:
 *
 * ```
 * void my_completion(irp_t* irp, void* ctx)
 * {
 *     // Do stuff...
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
 *     // Do stuff...
 *
 *     return EOK;
 * }
 * ```
 *
 * ## Error Values
 *
 * The IRP system uses the `err` field to indicate both the current state of the IRP as well as any error that may have
 * occurred during its processing.
 *
 * Included below are a list of "special" values which the IRP system will set:
 *
 * - `EOK`: Operation completed successfully.
 * - `ECANCELED`: Operation was cancelled.
 * - `ETIMEDOUT`: Operation timed out.
 *
 * @see kernel_io for the ring system.
 * @see [Wikipedia](https://en.wikipedia.org/wiki/I/O_request_packet) for more information about IRPs.
 * @see [Microsoft _IRP](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_irp) for information
 * on how Windows NT implements IRPs.
 * @{
 */

/**
 * @brief IRP complete callback type.
 *
 * @param irp The IRP.
 * @param ctx The contxt pointer from the `irp_frame_t` structure.
 */
typedef void (*irp_complete_t)(irp_t* irp, void* ctx);

/**
 * @brief IRP cancellation callback type.
 *
 * @param irp The IRP.
 * @return On success, `OK`. On failure, a non-zero status.
 */
typedef status_t (*irp_cancel_t)(irp_t* irp);

/**
 * @brief Sentinel value indicating that the IRP has been cancelled.
 */
#define IRP_CANCELLED ((irp_cancel_t)1)

typedef uint16_t irp_major_t;
#define IRP_MJ_READ 0
#define IRP_MJ_WRITE 1
#define IRP_MJ_POLL 2
#define IRP_MJ_MAX 3

typedef uint16_t irp_minor_t;
#define IRP_MN_NORMAL 0

#define IRP_ARGS_MAX 4 ///< The maximum number of 64-bit arguments in an `irp_frame_t`.

/**
 * @brief IRP stack frame structure.
 * @struct irp_frame_t
 */
typedef struct irp_frame
{
    irp_major_t major; ///< Major function number.
    irp_minor_t minor; ///< Minor function number.
    uint8_t _reserved[4];
    irp_complete_t complete; ///< Completion callback.
    void* ctx;               ///< Local context.
    vnode_t* vnode;          ///< Vnode associated with the operation.
    union {
        struct
        {
            file_t* file;
            mdl_t* buffer;
            size_t count;
            size_t offset;
        } read;
        struct
        {
            file_t* file;
            mdl_t* buffer;
            size_t count;
            size_t offset;
        } write;
        struct
        {
            file_t* file;
            io_events_t events;
        } poll;
        uint64_t args[IRP_ARGS_MAX]; ///< Generic arguments.
    };
} irp_frame_t;

static_assert(sizeof(irp_frame_t) == 64, "irp_frame_t is not 64 bytes");

#define IRP_FRAME_MAX 5 ///< The maximum number of frames in a IRP stack.

/**
 * @brief I/O Request Packet structure.
 * @struct irp_t
 *
 * The I/O Request Packet structure is designed to preallocate as much as possible such that in the common case there is
 * no need for any allocation beyond the allocation of the IRP itself. This does require careful consideration of
 * padding, alignment and field sizes to keep it within a reasonable size.
 *
 * @see kernel_io for more information for each possible verb.
 */
typedef struct ALIGNED(64) irp
{
    list_entry_t entry;           ///< Used to store the IRP in various lists.
    list_entry_t timeoutEntry;    ///< Used to store the IRP in the timeout queue.
    _Atomic(irp_cancel_t) cancel; ///< Cancellation callback, must be atomic to ensure an IRP is only cancelled once.
    clock_t deadline;             ///< The time at which the IRP will be removed from a timeout queue.
    mdl_t mdl;                    ///< A preallocated memory descriptor list for use by the IRP.
    union {
        size_t read;
        size_t write;
        uint64_t _raw;
    } res;
    status_t status;  ///< The status of the operation, also used to specify its current state.
    pool_idx_t index; ///< Index of the IRP in its pool.
    pool_idx_t next;  ///< Index of the next IRP in a chain or in the free list.
    cpu_id_t cpu;     ///< The CPU whose timeout queue the IRP is in.
    uint8_t frame;    ///< The index of the current frame in the stack.
    uint8_t reserved[5];
    irp_frame_t stack[IRP_FRAME_MAX]; ///< The frame stack, grows downwards.
    sqe_t sqe;                        // A copy of the submission queue entry associated with this IRP.
} irp_t;

static_assert(sizeof(irp_t) == 512, "irp_t is not 512 bytes");

/**
 * @brief Request pool structure.
 * @struct irp_pool
 */
typedef struct irp_pool
{
    void* ctx;
    process_t* process; ///< Will only hold a reference if there is at least one active IRP.
    atomic_size_t active;
    pool_t pool;
    size_t size;
    irp_t irps[];
} irp_pool_t;

/**
 * @brief IRP function type.
 */
typedef void (*irp_func_t)(irp_t* irp);

/**
 * @brief IRP vtable structure.
 * @struct irp_vtable_t
 */
typedef struct irp_vtable
{
    irp_func_t funcs[IRP_MJ_MAX];
} irp_vtable_t;

/**
 * @brief Allocate a new IRP pool.
 *
 * @param out Output pointer for the pool.
 * @param size The amount of requests to allocate.
 * @param process The process that will own the IRPs allocated from this pool.
 * @param ctx The context of the IRP pool.
 * @return An appropriate status value.
 */
status_t irp_pool_new(irp_pool_t** out, size_t size, process_t* process, void* ctx);

/**
 * @brief Free a IRP pool.
 *
 * @param pool The IRP pool to free.
 */
void irp_pool_free(irp_pool_t* pool);

/**
 * @brief Attempt to cancel all IRPs in a pool.
 *
 * @param pool The IRP pool.
 */
void irp_pool_cancel_all(irp_pool_t* pool);

/**
 * @brief Add an IRP to a per-CPU timeout queue.
 *
 * @param irp The IRP to add.
 * @param timeout The timeout of the IRP.
 */
void irp_timeout_add(irp_t* irp, clock_t timeout);

/**
 * @brief Remove an IRP from its per-CPU timeout queue.
 *
 * @param irp The IRP to remove.
 */
void irp_timeout_remove(irp_t* irp);

/**
 * @brief Check and handle expired IRP timeouts on the current CPU.
 */
void irp_timeouts_check(void);

/**
 * @brief Retrieve an inactive IRP from an IRP pool.
 *
 * The pool that the IRP is part off can be retrieved using the `irp_get_pool()` function.
 *
 * @param out Output pointer for the IRP.
 * @param pool The IRP pool.
 * @return An appropriate status value.
 */
status_t irp_get(irp_t** out, irp_pool_t* pool);

/**
 * @brief Retrieve a memory descriptor list and associate it with an IRP.
 *
 * All MDLs associated with a IRP will be cleaned up when finished.
 *
 * @param irp The IRP to associate the MDL with.
 * @param out Output pointer for the MDL.
 * @param addr The virtual address of the memory region to add to the MDL, or `NULL` for a blank MDL.
 * @param size The size of the memory region.
 * @return An appropriate status value.
 */
status_t irp_get_mdl(irp_t* irp, mdl_t** out, const void* addr, size_t size);

/**
 * @brief Retrieve the IRP pool that an IRP was allocated from.
 *
 * @param irp The IRP.
 * @return The IRP pool.
 */
static inline irp_pool_t* irp_get_pool(irp_t* irp)
{
    return CONTAINER_OF(irp, irp_pool_t, irps[irp->index]);
}

/**
 * @brief Retrieve the context of the IRP pool that an IRP was allocated from.
 *
 * @param irp The IRP.
 * @return The context.
 */
static inline void* irp_get_ctx(irp_t* irp)
{
    return irp_get_pool(irp)->ctx;
}

/**
 * @brief Retrieve the process that owns an IRP.
 *
 * @param irp The IRP.
 * @return The process.
 */
static inline process_t* irp_get_process(irp_t* irp)
{
    return irp_get_pool(irp)->process;
}

/**
 * @brief Retrieve the next IRP in a chain and advance the chain.
 *
 * @param irp The current IRP.
 * @return The next IRP, or `NULL` if there is no next IRP.
 */
static inline irp_t* irp_chain_next(irp_t* irp)
{
    irp_pool_t* pool = irp_get_pool(irp);
    if (irp->next == POOL_IDX_MAX)
    {
        return NULL;
    }

    irp_t* next = &pool->irps[irp->next];
    irp->next = next->next;
    next->next = POOL_IDX_MAX;
    return next;
}

/**
 * @brief Attempt to cancel an IRP.
 *
 * @param irp The IRP to cancel.
 * @return An appropriate status value.
 */
status_t irp_cancel(irp_t* irp);

/**
 * @brief Set the cancellation callback for an IRP.
 *
 * @param irp The IRP.
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
 * @brief Retrieve the current frame in the IRP stack.
 *
 * @param irp The IRP to retrieve the frame from.
 * @return The current frame.
 */
static inline irp_frame_t* irp_current(irp_t* irp)
{
    assert(irp->frame < IRP_FRAME_MAX);
    return &irp->stack[irp->frame];
}

/**
 * @brief Retrieve the next frame in the IRP stack.
 *
 * @param irp The IRP to retrieve the frame from.
 * @return The next frame, or `NULL` if we are at the bottom of the stack.
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
 * @brief Copy the current frame in the IRP stack to the next.
 *
 * @param irp The IRP.
 */
static inline void irp_copy_to_next(irp_t* irp)
{
    irp_frame_t* current = irp_current(irp);
    irp_frame_t* next = irp_next(irp);

    if (next->vnode != NULL)
    {
        UNREF(next->vnode);
        next->vnode = NULL;
    }

    *next = *current;
    next->vnode = NULL;
    next->complete = NULL;
    next->ctx = NULL;
}

/**
 * @brief Skip the current stack frame, meaning the next call will run in the same stack frame.
 *
 * @param irp The IRP.
 */
static inline void irp_skip(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    if (frame->vnode != NULL)
    {
        UNREF(frame->vnode);
        frame->vnode = NULL;
    }
    assert(irp->frame < IRP_FRAME_MAX);
    irp->frame++;
}

/**
 * @brief Send an IRP to a specified vnode.
 *
 * Will advance the IRP stack.
 *
 * @param irp The IRP to send.
 * @param vnode The vnode to associated with the next IRP stack frame.
 */
void irp_call(irp_t* irp, vnode_t* vnode);

/**
 * @brief Send an IRP to a specified function directly.
 *
 * Will advance the IRP stack.
 *
 * @param irp The IRP to send.
 * @param func The function to call.
 */
void irp_call_direct(irp_t* irp, irp_func_t func);

/**
 * @brief Complete the current frame in the IRP stack.
 *
 * If the current frame does not have a completion, it will automatically complete the next frame in the stack.
 *
 * If the last frame is reached, the IRP is considered finished. Which will causing its resources to be freed and for
 * the IRP to be returned to its pool.
 *
 * @param irp The IRP to complete.
 * @param status The status of the completed operation, if `OK` then the previous status is kept.
 */
void irp_complete(irp_t* irp, status_t status);

/**
 * @brief Set the completion callback and context for the next frame in the IRP stack.
 *
 * @param irp The IRP to set.
 * @param complete The completion callback.
 * @param ctx The context pointer to pass to the completion callback.
 */
static inline void irp_set_complete(irp_t* irp, irp_complete_t complete, void* ctx)
{
    irp_frame_t* next = irp_next(irp);
    next->complete = complete;
    next->ctx = ctx;
}

/**
 * @brief Prepares the next IRP stack frame for a generic operation.
 *
 * @param irp The IRP.
 * @param major The major function number.
 * @param arg0 Generic argument 0.
 * @param arg1 Generic argument 1.
 * @param arg2 Generic argument 2.
 * @param arg3 Generic argument 3.
 */
static inline void irp_prep_generic(irp_t* irp, irp_major_t major, uint64_t arg0, uint64_t arg1, uint64_t arg2,
    uint64_t arg3)
{
    irp_frame_t* next = irp_next(irp);
    assert(next != NULL);

    next->major = major;
    next->args[0] = arg0;
    next->args[1] = arg1;
    next->args[2] = arg2;
    next->args[3] = arg3;
}

/**
 * @brief Prepares the next IRP stack frame for a read operation.
 *
 * @param irp The IRP.
 * @param file The file to read from, will not take a new reference.
 * @param buffer The memory descriptor list to read into.
 * @param count The number of bytes to read.
 * @param offset The offset in the file to read from.
 */
static inline void irp_prep_read(irp_t* irp, file_t* file, mdl_t* buffer, size_t count, size_t offset)
{
    irp_frame_t* next = irp_next(irp);
    assert(next != NULL);

    next->major = IRP_MJ_READ;
    next->read.file = file;
    next->read.buffer = buffer;
    next->read.count = count;
    next->read.offset = offset;
}

/**
 * @brief Prepares the next IRP stack frame for a write operation.
 *
 * @param irp The IRP.
 * @param file The file to write to, will not take a new reference.
 * @param buffer The memory descriptor list to write from.
 * @param count The number of bytes to write.
 * @param offset The offset in the file to write to.
 */
static inline void irp_prep_write(irp_t* irp, file_t* file, mdl_t* buffer, size_t count, size_t offset)
{
    irp_frame_t* next = irp_next(irp);
    assert(next != NULL);

    next->major = IRP_MJ_WRITE;
    next->write.file = file;
    next->write.buffer = buffer;
    next->write.count = count;
    next->write.offset = offset;
}

/**
 * @brief Prepares the next IRP stack frame for a poll operation.
 *
 * @param irp The IRP.
 * @param file The file to poll, will not take a new reference.
 * @param events The events to wait for.
 */
static inline void irp_prep_poll(irp_t* irp, file_t* file, io_events_t events)
{
    irp_frame_t* next = irp_next(irp);
    assert(next != NULL);

    next->major = IRP_MJ_POLL;
    next->poll.file = file;
    next->poll.events = events;
}

/** @} */