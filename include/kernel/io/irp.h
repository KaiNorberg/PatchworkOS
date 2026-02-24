#pragma once

#include <kernel/mem/mdl.h>
#include <kernel/mem/pool.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/ref.h>

#include <assert.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/fs.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/status.h>
#include <time.h>

typedef struct file file_t;
typedef struct process process_t;
typedef struct vnode vnode_t;
typedef struct dentry dentry_t;

typedef struct irp irp_t;

/**
 * @brief I/O Request Packet.
 * @defgroup kernel_io_irp I/O Request Packet
 * @ingroup kernel_io
 *
 * The I/O Request Packet (IRP) is a lock-less, self-contained and layered packet that allow requests to be sent to
 * various subsystems.
 *
 * These requests can be processed immediately or, if if the subsystem is unable to complete the IRP immediately, the
 * self-contained nature of the IRP allows it to be stored and processed at a later time. This is what enables the
 * kernel to act asynchronously without blocking the calling thread.
 *
 * The IRP is designed to be generic enough to be used by any system in the kernel, however it is primarily used by the
 * I/O ring system.
 *
 * @warning While the cancellation or completion of an IRP is thread safe, the setup of an IRP is not (as in pushing
 * layers to it). It is assumed that only one thread is manipulating an IRP during its setup.
 *
 * ## Stack
 *
 * The IRP contains a stack of frames (`irp_frame_t`) which act like a call stack. Each frame contains the parameters to
 * be passed to some function, most often specified using a major and minor number, along with a completion routine to
 * be executed once the operation is completed. The result of each operation is stored in the `irp_t::result` and
 * `irp_t::status` fields.
 *
 * @note A `irp_call()` function is provided to directly call a function with an IRP. However, usually an IRP will be
 * dispatched to either a vnode or a file which contain a table of handlers, one for each major function number.
 *
 * @see vnode_call
 * @see file_call
 *
 * ## Completion
 *
 * Completion is the process of unwinding the IRP stack, this occurs whether the operation was successful or otherwise.
 *
 * There are two ways that an IRP can be completed, immediately or delayed.
 *
 * To immediately complete an IRP simply return any status that is not an informational `ST_CODE_PENDING` or
 * `ST_CODE_COMPLETE` status from the function invoked by any of the "call" functions.
 *
 * To delay the completion of an IRP, return an informational `ST_CODE_PENDING` status. This indicates that the
 * subsystem has taken ownership of the IRP and will complete it at a later time by calling `irp_complete()`.
 *
 * To indicate that the IRP has already been completed, return an informational
 * `ST_CODE_COMPLETE` status. This is usefull if a handler invokes another handler, for example when `irp_call()`
 * completes an IRP it will return an informational `ST_CODE_COMPLETE` status, making sure that a lower `irp_call()`
 * does not try to complete the same IRP again.
 *
 * For convenience, the `irp_delay()` helper can be used to add an IRP to a list and a timeout queue, while also setting
 * a cancellation callback.
 *
 * ## Cancellation
 *
 * Cancelling an IRP can intuitively be considered equivalent to forcing the last completion to fail with an error
 * status, thus resulting in all the other completions to fail as well.
 *
 * The current owner of an IRP is responsible for handling cancellation by specifying a cancellation callback via
 * `irp_set_cancel()`. The current owner generally being the last subsystem or function to receive the IRP.
 *
 * One vital aspect of this system is the need to "claim" the IRP. When an IRP is "delayed", as in it is added to a
 * timeout queue or added to a queue for later processing, it is considered to be unowned. At this point, it may be
 * possible for multiple threads to attempt to cancel or complete the IRP.
 *
 * As such, we need to establish a new owner for the IRP which is then the only thread allowed to cancel or complete it
 * (unless it was already cancelled). This is done by atomically exchanging the cancel callback. If the callback is
 * exchanged with `IRP_CANCELLED`, the IRP is cancelled. If it is exchanged with `NULL` (via `irp_claim()`), the IRP is
 * claimed for completion.
 *
 * Finally, there is one more detail worth considering. Say we have the following cancellation callback:
 *
 * ```
 * status_t my_cancel(irp_t* irp)
 * {
 *     // At this point we are considered the owner of the IRP.
 *
 *     if (IS_CODE(irp->status, TIMEOUT))
 *     {
 *         // We timed out.
 *     }
 *     if (IS_CODE(irp->status, CANCELLED))
 *     {
 *         // We were explicitly cancelled.
 *     }
 *
 *     return OK;
 * }
 * ```
 *
 * Lets also say that simultaneously another thread is attempting to complete the IRP as follows:
 *
 * ```
 * void other_thread(void)
 * {
 *     irp_t* irp = ...;
 *     if (irp_claim(irp))
 *     {
 *         // We are now the owner. So we can safely complete the IRP, right?
 *         irp_complete(irp, OK);
 *     }
 * }
 * ```
 *
 * The above code contains a subtle race condition, which is that while we have claimed ownership of the IRP, thus
 * giving us the right to complete it, we might not own the actual memory in which its stored, giving us a
 * use-after-free bug.
 *
 * Consider that if the `my_cancel()` callback is being called and returns before the `irp_claim()` within
 * `other_thread()`, then the IRP might have been returned to its pool or even worse the pool might also have been
 * freed.
 *
 * It is thus vital that all subsystems ensure that when their cancellation callbacks return, that there are no other
 * threads that can access the IRP.
 *
 * In most cases avoiding the above race condition is as simple as storing the IRP in a lock protected list. For
 * example:
 *
 * ```
 * status_t my_cancel(irp_t* irp)
 * {
 *     lock_acquire(&my_list_lock);
 *     list_remove(&irp->entry);
 *     lock_release(&my_list_lock);
 *     return OK;
 * }
 *
 * void other_thread(void)
 * {
 *     lock_acquire(&my_list_lock);
 *     irp_t* irp = CONTAINER_OF(list_pop_front(&my_list), irp_t, entry);
 *     if (irp_claim(irp))
 *     {
 *         lock_release(&my_list_lock);
 *         irp_complete(irp, OK);
 *     }
 *     else
 *     {
 *         lock_release(&my_list_lock);
 *     }
 * }
 * ```
 *
 * Additionally, everything described above only applies to cancellable IRPs. As such, for certain subsystems or drivers
 * where it is not possible or reasonable to handle cancellation, one may simply not implement cancellation.
 *
 * ## Operations
 *
 * Each operation is specified by a "major function number", with each number having an associated argument structure of
 * the same name within the `irp_frame_t` structure.
 *
 * Each operation is expected to place its result into the generic `irp_t::result` field.
 *
 * @see kernel_io_ioring for the ring system.
 * @see [Wikipedia](https://en.wikipedia.org/wiki/I/O_request_packet) for more information about IRPs.
 * @see [Microsoft _IRP](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_irp) for information
 * on how Windows NT implements IRPs.
 * @{
 */

typedef uint16_t irp_major_t; ///< IRP major function number type.

/**
 * @brief Read operation.
 * @return The number of bytes read.
 */
#define IRP_MJ_READ 0

/**
 * @brief Write operation.
 * @return The number of bytes written.
 */
#define IRP_MJ_WRITE 1

/**
 * @brief Poll operation.
 * @result The events that occurred stored as a `events_t` value.
 */
#define IRP_MJ_POLL 2

/**
 * @brief Seek operation.
 * @return The new file position.
 */
#define IRP_MJ_SEEK 3

/**
 * @brief Memory map operation.
 * @return The virtual address where the file was mapped.
 */
#define IRP_MJ_MMAP 4

/**
 * @brief Control operation.
 * @return The result of the command.
 */
#define IRP_MJ_CONTROL 5

/**
 * @brief Open operation.
 * @return Always `0`.
 */
#define IRP_MJ_OPEN 6

/**
 * @brief Lookup operation.
 * @return Always `0`.
 */
#define IRP_MJ_LOOKUP 7

/**
 * @brief Remove operation.
 * @return Always `0`.
 */
#define IRP_MJ_REMOVE 8

/**
 * @brief Attribute operation.
 * @return If a get operation, the value of the attribute. If a set operation, always `0`.
 */
#define IRP_MJ_ATTR 9

/**
 * @brief Query operation.
 * @return Always `0`.
 */
#define IRP_MJ_QUERY 10

/**
 * @brief Flush operation.
 * @return Always `0`.
 */
#define IRP_MJ_FLUSH 11

#define IRP_MJ_MAX 12 ///< The maximum number of major function numbers.

typedef uint16_t irp_minor_t; ///< IRP minor function number type.
#define IRP_MN_NORMAL 0       ///< No special behaviour.

typedef uint16_t irp_flags_t;          ///< IRP frame flags type.
#define IRP_FLAG_NONE 0                ///< No flags.
#define IRP_FLAG_USE_FILE_POS (1 << 0) ///< If set, the operation will use and update the file's current position.

/**
 * @brief IRP function type.
 *
 * @param irp The IRP to send.
 * @result A informational `ST_CODE_PENDING` or `ST_CODE_COMPLETE` status value if the IRP was not completed
 * immediately, otherwise an appropriate status value.
 */
typedef status_t (*irp_handler_t)(irp_t* irp);

/**
 * @brief IRP complete callback type.
 *
 * @param irp The IRP.
 * @param ctx The context pointer from the `irp_frame_t` structure.
 * @result A informational `ST_CODE_PENDING` or `ST_CODE_COMPLETE` status value if the IRP requires more processing,
 * otherwise an appropriate status value.
 */
typedef status_t (*irp_complete_t)(irp_t* irp, void* ctx);

/**
 * @brief IRP cancellation callback type.
 *
 * @param irp The IRP.
 * @return An appropriate status code.
 */
typedef status_t (*irp_cancel_t)(irp_t* irp);

/**
 * @brief Sentinel value indicating that the IRP has been cancelled.
 */
#define IRP_CANCELLED ((irp_cancel_t)1)

#define IRP_ARGS_MAX 3 ///< The maximum number of 64-bit arguments in an `irp_frame_t`.

/**
 * @brief IRP stack frame structure.
 * @struct irp_frame_t
 *
 * @warning Generally, IRP frames should not be setup manually, instead helper functions such as `irp_prep_read()`,
 * `irp_prep_write()`, etc. should be used.
 */
typedef struct irp_frame
{
    irp_major_t major; ///< Major function number.
    irp_minor_t minor; ///< Minor function number.
    irp_flags_t flags; ///< Flags.
    uint8_t _reserved[2];
    irp_complete_t complete; ///< Completion callback.
    void* ctx;               ///< Local context.
    vnode_t* vnode;          ///< Vnode associated with the operation.
    file_t* file;            ///< File associated with the operation, can be `NULL`.
    union {
        struct
        {
            mdl_t* buffer;      ///< The MDL describing the buffer to read into.
            size_t* offset;     ///< The offset within the file to read from.
            size_t dummyOffset; ///< Allows the offset to be redirected to the file's current position.
        } read;
        struct
        {
            mdl_t* buffer;      ///< The MDL describing the buffer to write from.
            size_t* offset;     ///< The offset within the file to write to.
            size_t dummyOffset; ///< Allows the offset to be redirected to the file's current position.
        } write;
        struct
        {
            events_t events; ///< The events to poll for.
        } poll;
        struct
        {
            ssize_t offset;    ///< The offset to seek to.
            whence_t origin; ///< The origin of the seek operation.
        } seek;
        struct
        {
            void* address;     ///< The virtual address to map the file into, or `NULL` for any address.
            size_t offset;     ///< The offset within the file to start mapping from.
            uint32_t length;   ///< The number of bytes to map.
            pml_flags_t flags; ///< The paging flags to apply to the mapping.
        } mmap;
        struct
        {
            iocmd_t command;  ///< The command to perform.
            const char* args; ///< The arguments for the command.
        } control;
        struct
        {
            const void* payload; ///< Payload data for the open operation.
            size_t payloadLen;   ///< The length of the payload data.
        } open;
        struct
        {
            dentry_t* dentry; ///< The negative dentry to fill.
        } lookup;
        struct
        {
            dentry_t* dentry; ///< The dentry to remove.
        } remove;
        struct
        {
            file_attr_t attr;  ///< The attribute to get or set.
            uint64_t value; ///< The value to set.
        } attr;
        struct
        {
            mdl_t* buffer; ///< The buffer to write the `file_info_t` into.
        } query;
        uint64_t args[IRP_ARGS_MAX]; ///< Generic arguments.
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
 * @see kernel_io for more information for each possible verb.
 */
typedef struct irp
{
    list_entry_t entry;           ///< Used to store the IRP in various lists.
    list_entry_t timeoutEntry;    ///< Used to store the IRP in the timeout queue.
    _Atomic(irp_cancel_t) cancel; ///< Cancellation callback, must be atomic to ensure an IRP is only cancelled once.
    union {
        clock_t timeout;  ///< The timeout of the operation starting from when the IRP is added to a timeout queue.
        clock_t deadline; ///< The time at which the IRP will be removed from a timeout queue.
    };
    mdl_t mdl;        ///< A preallocated memory descriptor list for use by the IRP.
    uintptr_t result; ///< The result returned by the last completed frame.
    status_t status;  ///< The status of the last completed frame.
    pool_idx_t index; ///< Index of the IRP in its pool.
    pool_idx_t next;  ///< Index of the next IRP in a chain or in the free list.
    cpu_id_t cpu;     ///< The CPU whose timeout queue the IRP is in.
    uint8_t loc;      ///< The index of the current frame in the stack.
    uint8_t _reserved[5];
    iosqe_t sqe;                      // A copy of the submission queue entry associated with this IRP.
    irp_frame_t stack[IRP_FRAME_MAX]; ///< The frame stack, grows downwards.
} ALIGNED(64) irp_t;

/**
 * @brief Request pool structure.
 * @struct irp_pool_t
 */
typedef struct irp_pool
{
    void* ctx;
    process_t* process; ///< Will only hold a reference if there is at least one active IRP.
    atomic_size_t active;
    pool_t pool;
    size_t size;
    irp_t irps[] ALIGNED(64);
} irp_pool_t;

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
 * The timeout of the IRP is specified in `irp->timeout`.
 *
 * @param irp The IRP to add.
 * @param cancel The cancellation callback to set.
 * @return An appropriate status value.
 */
status_t irp_timeout_add(irp_t* irp, irp_cancel_t cancel);

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
 * @param pool The IRP pool.
 * @param out Output pointer for the IRP.
 * @return An appropriate status value.
 */
status_t irp_get(irp_pool_t* pool, irp_t** out);

/**
 * @brief Retrieve a memory descriptor list and associate it with an IRP.
 *
 * All MDLs associated with a IRP will be cleaned up when finished.
 *
 * @param irp The IRP to associate the MDL with.
 * @param out Output pointer for the MDL.
 * @return An appropriate status value.
 */
status_t irp_get_mdl(irp_t* irp, mdl_t** out);

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
 * @brief Retrieve the next IRP in a chain and clear its next pointer.
 *
 * @param irp The current IRP.
 * @return The next IRP, or `NULL` if there is no next IRP.
 */
static inline irp_t* irp_chain_next(irp_t* irp)
{
    if (irp->next == POOL_IDX_MAX)
    {
        return NULL;
    }

    irp_pool_t* pool = irp_get_pool(irp);
    irp_t* next = &pool->irps[irp->next];
    irp->next = POOL_IDX_MAX;
    return next;
}

/**
 * @brief Retrieve the current frame in the IRP stack.
 *
 * @param irp The IRP to retrieve the frame from.
 * @return The current frame.
 */
static inline irp_frame_t* irp_current(irp_t* irp)
{
    assert(irp->loc < IRP_FRAME_MAX);
    return &irp->stack[irp->loc];
}

/**
 * @brief Retrieve the next frame in the IRP stack.
 *
 * @param irp The IRP to retrieve the frame from.
 * @return The next frame, or `NULL` if we are at the bottom of the stack.
 */
static inline irp_frame_t* irp_next(irp_t* irp)
{
    if (irp->loc == 0)
    {
        return NULL;
    }
    return &irp->stack[irp->loc - 1];
}

/**
 * @brief Send an IRP to a specified function directly.
 *
 * Will advance the IRP stack.
 *
 * @param irp The IRP to send.
 * @param func The function to call.
 * @return An appropriate status value.
 */
status_t irp_call(irp_t* irp, irp_handler_t func);

/**
 * @brief Complete the current frame in the IRP stack.
 *
 * If the current frame does not have a completion, it will automatically complete the next frame in the stack.
 *
 * If the last frame is reached, the IRP is considered finished. Which will causing its resources to be freed and
 * for the IRP to be returned to its pool.
 *
 * @param irp The IRP to complete.
 * @param status The status of the completed operation, if an informational `ST_CODE_PENDING` or `ST_CODE_COMPLETE` than
 * this becomes a no-op.
 */
void irp_complete(irp_t* irp, status_t status);

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
 * @note It is generally preferred to use `irp_timeout_add()` over this function, `irp_set_cancel()` should only be used
 * when timeouts are not desired.
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
 * @brief Claim an IRP, ensuring that it is not already cancelled or being cancelled.
 *
 * After a call to this function it is guaranteed that no other thread will attempt to complete or cancel the IRP.
 *
 * @warning It is the responsibility of the caller to ensure that it is not possible for an IRP to be atomically
 * cancelled and free while this function is being called.
 *
 * @param irp The IRP to claim.
 * @return `true` if the IRP was successfully claimed, `false` otherwise.
 */
static inline WARN_UNUSED_RESULT bool irp_claim(irp_t* irp)
{
    return irp_set_cancel(irp, NULL) != IRP_CANCELLED;
}

/**
 * @brief Claim a list of IRPs, moving all successfully claimed IRPs to another list.
 *
 * Any IRP that was not claimed will be removed from the source list.
 *
 * @warning It is the responsibility of the caller to ensure the source and destination lists are protected and that it
 * is not possible for an IRP to be atomically cancelled and freed while this function is being called.
 *
 * @param dest The destination list.
 * @param src The source list.
 */
static inline void irp_claim_list(list_t* dest, list_t* src)
{
    while (!list_is_empty(src))
    {
        irp_t* irp = CONTAINER_OF(list_pop_front(src), irp_t, entry);
        if (irp_claim(irp))
        {
            list_move(dest, &irp->entry);
        }
    }
}

/**
 * @brief Add an IRP to a list to be handled later.
 *
 * Adds the IRP to the specified list, sets the cancellation callback and adds it to a per-CPU timeout queue.
 *
 * @warning The caller is responsible for protecting the list, for example, using a lock. Additionally, when the IRP is
 * later completed it must first be claimed using `irp_claim()` or `irp_claim_list()` to ensure multiple threads do not
 * attempt to complete/cancel the same IRP.
 *
 * @param irp The IRP to delay.
 * @param list The list to add the IRP to.
 * @param cancel The cancellation callback.
 * @return An appropriate status value, if an error occurs during setup, the IRP is not added to the list.
 */
static inline status_t irp_delay(irp_t* irp, list_t* list, irp_cancel_t cancel)
{
    if (irp->timeout == 0)
    {
        return ERR(IO, TIMEOUT);
    }

    list_push_back(list, &irp->entry);
    status_t status = irp_timeout_add(irp, cancel);
    if (IS_ERR(status))
    {
        list_remove(&irp->entry);
        return status;
    }

    return INFO(IO, PENDING);
}

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
 * @brief Helper function for implementing a read operation from a buffer into a read IRP's MDL.
 *
 * @param irp The IRP.
 * @param buffer The source buffer.
 * @param size The size of the source buffer.
 * @return An appropriate status value.
 */
status_t irp_read_helper(irp_t* irp, const void* buffer, size_t size);

/**
 * @brief Helper function for implementing a write operation from a write IRP's MDL into a buffer.
 *
 * @param irp The IRP.
 * @param buffer The destination buffer.
 * @param size The size of the destination buffer.
 * @return An appropriate status value.
 */
status_t irp_write_helper(irp_t* irp, void* buffer, size_t size);

/**
 * @brief Prepares the next IRP stack frame for a read operation.
 *
 * @see `IRP_MJ_READ`
 */
static inline void irp_prep_read(irp_t* irp, mdl_t* buffer, ssize_t offset)
{
    irp_frame_t* next = irp_next(irp);
    assert(next != NULL);

    next->major = IRP_MJ_READ;
    next->minor = IRP_MN_NORMAL;
    next->flags = IRP_FLAG_NONE;
    next->read.buffer = buffer;
    next->read.dummyOffset = offset;
    next->read.offset = &next->read.dummyOffset;
    if (offset == IOCUR)
    {
        next->flags |= IRP_FLAG_USE_FILE_POS;
    }
}

/**
 * @brief Prepares the next IRP stack frame for a write operation.
 *
 * @see `IRP_MJ_WRITE`
 */
static inline void irp_prep_write(irp_t* irp, mdl_t* buffer, ssize_t offset)
{
    irp_frame_t* next = irp_next(irp);
    assert(next != NULL);

    next->major = IRP_MJ_WRITE;
    next->minor = IRP_MN_NORMAL;
    next->flags = IRP_FLAG_NONE;
    next->write.buffer = buffer;
    next->write.dummyOffset = offset;
    next->write.offset = &next->write.dummyOffset;
    if (offset == IOCUR)
    {
        next->flags |= IRP_FLAG_USE_FILE_POS;
    }
}

/**
 * @brief Prepares the next IRP stack frame for a poll operation.
 *
 * @see `IRP_MJ_POLL`
 */
static inline void irp_prep_poll(irp_t* irp, events_t events)
{
    irp_frame_t* next = irp_next(irp);
    assert(next != NULL);

    next->major = IRP_MJ_POLL;
    next->minor = IRP_MN_NORMAL;
    next->flags = IRP_FLAG_NONE;
    next->poll.events = events;
}

/**
 * @brief Prepares the next IRP stack frame for a seek operation.
 *
 * @see `IRP_MJ_SEEK`
 */
static inline void irp_prep_seek(irp_t* irp, ssize_t offset, whence_t origin)
{
    irp_frame_t* next = irp_next(irp);
    assert(next != NULL);

    next->major = IRP_MJ_SEEK;
    next->minor = IRP_MN_NORMAL;
    next->flags = IRP_FLAG_NONE;
    next->seek.offset = offset;
    next->seek.origin = origin;
}

/**
 * @brief Prepares the next IRP stack frame for a memory map operation.
 *
 * @see `IRP_MJ_MMAP`
 */
static inline void irp_prep_mmap(irp_t* irp, void* address, size_t length, size_t offset, pml_flags_t flags)
{
    irp_frame_t* next = irp_next(irp);
    assert(next != NULL);

    next->major = IRP_MJ_MMAP;
    next->minor = IRP_MN_NORMAL;
    next->flags = IRP_FLAG_NONE;
    next->mmap.address = address;
    next->mmap.length = length;
    next->mmap.offset = offset;
    next->mmap.flags = flags;
}

/**
 * @brief Prepares the next IRP stack frame for a control operation.
 *
 * @see `IRP_MJ_CONTROL`
 */
static inline void irp_prep_control(irp_t* irp, iocmd_t command, const char* args)
{
    irp_frame_t* next = irp_next(irp);
    assert(next != NULL);

    next->major = IRP_MJ_CONTROL;
    next->minor = IRP_MN_NORMAL;
    next->flags = IRP_FLAG_NONE;
    next->control.command = command;
    next->control.args = args;
}

/**
 * @brief Prepares the next IRP stack frame for an open operation.
 *
 * @see `IRP_MJ_OPEN`
 */
static inline void irp_prep_open(irp_t* irp, const void* payload, size_t payloadLen)
{
    irp_frame_t* next = irp_next(irp);
    assert(next != NULL);

    next->major = IRP_MJ_OPEN;
    next->minor = IRP_MN_NORMAL;
    next->flags = IRP_FLAG_NONE;
    next->open.payload = payload;
    next->open.payloadLen = payloadLen;
}

/**
 * @brief Prepares the next IRP stack frame for a lookup operation.
 *
 * @see `IRP_MJ_LOOKUP`
 */
static inline void irp_prep_lookup(irp_t* irp, dentry_t* dentry)
{
    irp_frame_t* next = irp_next(irp);
    assert(next != NULL);

    next->major = IRP_MJ_LOOKUP;
    next->minor = IRP_MN_NORMAL;
    next->flags = IRP_FLAG_NONE;
    next->lookup.dentry = dentry;
}

/**
 * @brief Prepares the next IRP stack frame for a remove operation.
 *
 * @see `IRP_MJ_REMOVE`
 */
static inline void irp_prep_remove(irp_t* irp, dentry_t* dentry)
{
    irp_frame_t* next = irp_next(irp);
    assert(next != NULL);

    next->major = IRP_MJ_REMOVE;
    next->minor = IRP_MN_NORMAL;
    next->flags = IRP_FLAG_NONE;
    next->remove.dentry = dentry;
}

/**
 * @brief Prepares the next IRP stack frame for an attribute operation.
 *
 * @see `IRP_MJ_ATTR`
 */
static inline void irp_prep_attr(irp_t* irp, file_attr_t attr, uint64_t value)
{
    irp_frame_t* next = irp_next(irp);
    assert(next != NULL);

    next->major = IRP_MJ_ATTR;
    next->minor = IRP_MN_NORMAL;
    next->flags = IRP_FLAG_NONE;
    next->attr.attr = attr;
    next->attr.value = value;
}

/**
 * @brief Prepares the next IRP stack frame for a query operation.
 *
 * @see `IRP_MJ_QUERY`
 */
static inline void irp_prep_query(irp_t* irp, mdl_t* buffer)
{
    irp_frame_t* next = irp_next(irp);
    assert(next != NULL);

    next->major = IRP_MJ_QUERY;
    next->minor = IRP_MN_NORMAL;
    next->flags = IRP_FLAG_NONE;
    next->query.buffer = buffer;
}

/**
 * @brief Prepares the next IRP stack frame for a flush operation.
 *
 * @see `IRP_MJ_FLUSH`
 */
static inline void irp_prep_flush(irp_t* irp)
{
    irp_frame_t* next = irp_next(irp);
    assert(next != NULL);

    next->major = IRP_MJ_FLUSH;
    next->minor = IRP_MN_NORMAL;
    next->flags = IRP_FLAG_NONE;
}

/** @} */
