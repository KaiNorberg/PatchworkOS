#pragma once

#include <kernel/config.h>
#include <kernel/io/irp.h>
#include <kernel/log/panic.h>
#include <kernel/mem/mdl.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>

#include <string.h>
#include <sys/io.h>

/**
 * @brief Kernel-side I/O ring interface.
 * @defgroup kernel_io_ioring_get Kernel-side I/O Ring Interface
 * @ingroup kernel_io
 *
 * The I/O ring provides the core of all interfaces in PatchworkOS, where user-space submits Submission Queue Entries
 * (SQEs) and receives Completion Queue Entries (CQEs) from it, all within shared memory. Allowing for highly efficient
 * and asynchronous I/O operations, especially since PatchworkOS is designed to be natively asynchronous with its I/O
 * Request Packet system.
 *
 * Each SQE specifies an operation to perform and a set of up to `IOSQE_MAX_ARG` arguments, while each CQE returns the
 * result of a previously submitted SQE.
 *
 * Synchronous operations are implemented on top of this API in userspace.
 *
 * @see libstd_sys_io for the list of operations and their arguments.
 * @see [Wikipedia](https://en.wikipedia.org/wiki/Io_uring) for information about `io_uring`, the inspiration for this
 * system.
 * @see [Manpages](https://man7.org/linux/man-pages/man7/io_uring.7.html) for more information about `io_uring`.
 *
 * ## Syncronization
 *
 * The I/O ring structure is designed to be safe under the assumption that there is a single producer (one user-space
 * thread) and a single consumer (the kernel).
 *
 * If an I/O ring needs multiple producers (needs to be accessed by multiple threads) it is the responsibility of
 * the caller to ensure proper synchronization.
 *
 * @todo This might be changed in the future to allow for multiple producers/consumers.
 *
 * Regarding the I/O ring structure itself, the structure can only be torndown as long as nothing is using it and there
 * are no pending operations.
 *
 * ## Registers
 *
 * Operations performed on a I/O ring can load arguments from, and save their results to, seven 64-bit virtual
 * registers. All registers are stored in the shared control area of the I/O ring structure (`ioring_ctrl_t`), as such
 * they can be inspected and modified by user space.
 *
 * When a SQE is processed, the kernel will check six register specifiers in the SQE flags, one for each argument and
 * one for the result. Each specifier is stored as three bits, with a `IOSQE_REG_NONE` value indicating no register.
 *
 * The offset of the specifier specifies its meaning, for example, bits `0-2` specify the register to load into the
 * first argument, bits `3-5` specify the register to load into the second argument, and so on until bits `15-17` which
 * specify the register to save the result into.
 *
 * This system, when combined with `IOSQE_LINK`, allows for multiple operations to be performed at once, for example, it
 * would be possible to open a file, read from it, seek to a new position, write to it, and finally close the file, with
 * a single `ioring_enter()` call.
 *
 * @see `iosqe_flags_t` for more information about register specifiers and their formatting.
 *
 * ## Arguments
 *
 * Arguments within a SQE are stored in five 64-bit values, `arg0` through `arg4`. For convenience, each argument value
 * is stored as a union with various types.
 *
 * ## Results
 *
 * The result of a SQE is stored in its corresponding CQE using a single 64-bit value along side a `status_t` status
 * code.
 *
 * @{
 */

/**
 * @brief Ring context flags.
 * @enum ioring_ctx_flags_t
 */
typedef enum
{
    IORING_CTX_NONE = 0,        ///< No flags set.
    IORING_CTX_BUSY = 1 << 0,   ///< Context is currently being used, used for fast locking.
    IORING_CTX_MAPPED = 1 << 1, ///< Context is currently mapped into userspace.
} ioring_ctx_flags_t;

/**
 * @brief The kernel-side ring context structure.
 * @struct ioring_ctx_t
 */
typedef struct ioring_ctx
{
    ioring_t ring;             ///< The kernel-side ring structure.
    process_t* process;        ///< The process that owns this ring.
    list_t active;             ///< List of active IRPs.
    lock_t lock;               ///< Lock for the active list.
    atomic_size_t activeCount; ///< Number of active IRPs.
    void* userAddr;            ///< Userspace address of the ring.
    void* kernelAddr;          ///< Kernel address of the ring.
    size_t pageAmount;         ///< Amount of pages mapped for the ring.
    wait_queue_t waitQueue;    ///< Wait queue for completions.
    _Atomic(ioring_ctx_flags_t) flags;
} ioring_ctx_t;

/**
 * @brief Initialize a I/O context.
 *
 * @param ctx Pointer to the context to initialize.
 */
void ioring_ctx_init(ioring_ctx_t* ctx);

/**
 * @brief Deinitialize a I/O context.
 *
 * @param ctx Pointer to the context to deinitialize.
 */
void ioring_ctx_deinit(ioring_ctx_t* ctx);

/** @} */