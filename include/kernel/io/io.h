#pragma once

#include <kernel/config.h>
#include <kernel/log/panic.h>
#include <kernel/mem/mem_desc.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/wait.h>
#include <kernel/io/irp.h>
#include <kernel/sync/lock.h>

#include <string.h>
#include <sys/ioring.h>

/**
 * @brief Programmable submission/completion interface.
 * @defgroup kernel_io Kernel-side I/O Ring Interface
 * @ingroup kernel
 *
 * @todo The I/O ring system is primarily a design document for now as it remains very work in progress and subject to
 * change, currently being mostly unimplemented.
 *
 * The I/O ring provides the core of all interfaces in PatchworkOS, all implemented in an interface
 * inspired by `io_uring()` from Linux.
 *
 * Synchronous operations are implemented on top of this API in userspace.
 *
 * @see libstd_sys_ioring for the userspace interface to the asynchronous ring.
 * @see [Wikipedia](https://en.wikipedia.org/wiki/Io_uring) for information about `io_uring`.
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
 * @note The reason for this limitation is optimization for the common case, as the syncronization logic for multiple
 * producers would add significant overhead.
 *
 * Regarding the I/O ring structure itself, the structure can only be torndown as long as nothing is using it and there are
 * no pending operations.
 *
 * ## Registers
 *
 * Operations performed on a I/O ring can load arguments from, and save their results to, seven 64-bit general purpose
 * registers. All registers are stored in the shared control area of the I/O ring structure (`ioring_ctrl_t`), as such they can be inspected and
 * modified by user space.
 *
 * When a SQE is processed, the kernel will check six register specifiers in the SQE flags, one for each argument and
 * one for the result. Each specifier is stored as three bits, with a `SQE_REG_NONE` value indicating no-op and any
 * other value representing the n-th register. The offset of the specifier specifies its meaning, for example, bits
 * `0-2` specify the register to load into the first argument, bits `3-5` specify the register to load into the second
 * argument, and so on until bits `15-17` which specify the register to save the result into.
 *
 * This system, when combined with `SQE_LINK`, allows for multiple operations to be performed at once, for example, it
 * would be possible to open a file, read from it, seek to a new position, write to it, and finally close the file, with
 * a single `enter()` call.
 *
 * @see `sqe_flags_t` for more information about register specifiers and their formatting.
 *
 * ## Errors
 *
 * The majority of errors are returned in the completion queue entries, certain errors (such as `ENOMEM`) may be
 * reported directly from the `enter()` call.
 *
 * Certain error values that may be returned in a completion queue entry include:
 * - `EOK`: Success.
 * - `ECANCELED`: The operation was cancelled.
 * - `ETIMEDOUT`: The operation timed out.
 * - Other values may be returned depending on the operation.
 *
 * ## Verbs
 *
 * A verb specifies the operation to perform. Included is a list of currently defines verbs.
 *
 * ### `VERB_NOP`
 *
 * Never completes, can be used to implement a sleep equivalent by specifying a timeout.
 *
 * @param None
 * @return Always `0`.
 *
 * ### `VERB_OPEN`
 *
 * Opens a file, including regular files, directories, symlinks, etc.
 *
 * @param from The file descriptor to open the file relative to, or `FD_NONE` to open from the current working
 * directory.
 * @param path Pointer to a null-terminated string containing the path to the file to open
 * @return The file descriptor of the opened file.
 *
 * @{
 */

/**
 * @brief Ring context flags.
 * @enum io_ctx_flags_t
 */
typedef enum
{
    IO_CTX_NONE = 0,        ///< No flags set.
    IO_CTX_BUSY = 1 << 0,   ///< Context is currently being used, used for fast locking.
    IO_CTX_MAPPED = 1 << 1, ///< Context is currently mapped into userspace.
} io_ctx_flags_t;

/**
 * @brief The kernel-side ring context structure.
 * @struct io_ctx_t
 */
typedef struct io_ctx
{
    ioring_t ring;            ///< The kernel-side ring structure.
    irp_pool_t* irps;       ///< Pool of preallocated IRPs.
    mem_desc_pool_t* descs; ///< Pool of preallocated memory descriptors.
    void* userAddr;         ///< Userspace address of the ring.
    void* kernelAddr;       ///< Kernel address of the ring.
    size_t pageAmount;      ///< Amount of pages mapped for the ring.
    space_t* space;         ///< Pointer to the owning address space.
    wait_queue_t waitQueue; ///< Wait queue for completions.
    process_t* process;     ///< Holds a reference to the owner process while there are pending requests.
    _Atomic(io_ctx_flags_t) flags;
} io_ctx_t;

/**
 * @brief Initialize a I/O context.
 *
 * @param ctx Pointer to the context to initialize.
 */
void io_ctx_init(io_ctx_t* ctx);

/**
 * @brief Deinitialize a I/O context.
 *
 * @param ctx Pointer to the context to deinitialize.
 */
void io_ctx_deinit(io_ctx_t* ctx);

/**
 * @brief Notify the context of new SQEs.
 *
 * @param ctx Pointer to the context.
 * @param amount The number of SQEs to process.
 * @param wait The minimum number of CQEs to wait for.
 * @return On success, the number of SQEs processed. On failure, `ERR` and `errno` is set.
 */
uint64_t io_ctx_notify(io_ctx_t* ctx, size_t amount, size_t wait);

/** @} */