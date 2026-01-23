#pragma once

#include <kernel/config.h>
#include <kernel/io/irp.h>
#include <kernel/log/panic.h>
#include <kernel/mem/mdl.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/wait.h>
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
 * @todo Rewrite the Kernel-side I/O Ring Interface documentation to match the new system.
 * 
 * The I/O ring provides the core of all interfaces in PatchworkOS, where user-space submits Submission Queue Entries
 * (SQEs) and receives Completion Queue Entries (CQEs) from it, all within shared memory. Allowing for highly efficient
 * and asynchronous I/O operations, especially since PatchworkOS is designed to be natively asynchronous.
 *
 * Each SQE specifies a verb (the operation to perform) and a set of up to `SQE_MAX_ARG` arguments, while each CQE
 * returns the result of a previously submitted SQE.
 *
 * Synchronous operations are implemented on top of this API in userspace.
 *
 * @see libstd_sys_ioring for the userspace interface to the asynchronous ring.
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
 * @note The reason for this limitation is optimization for the common case, as the syncronization logic for multiple
 * producers would add significant overhead. Additionally, it is rather straight forward for user-space to protect the
 * ring with a mutex should it need to.
 *
 * Regarding the I/O ring structure itself, the structure can only be torndown as long as nothing is using it and there
 * are no pending operations.
 *
 * ## Registers
 *
 * Operations performed on a I/O ring can load arguments from, and save their results to, seven 64-bit general purpose
 * registers. All registers are stored in the shared control area of the I/O ring structure (`ioring_ctrl_t`), as such
 * they can be inspected and modified by user space.
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
 * ## Arguments
 *
 * Arguments within a SQE are stored in five 64-bit values, `arg1` through `arg5`. For convenience, each argument value
 * is stored as a union with various types.
 *
 * To avoid nameing conflicts and to avoid having to define new arguments for each verb, we define a convention to be
 * used for the arguments.
 *
 * - `arg0`: The noun or subject of the verb, for example, a `fd_t` for file operations.
 * - `arg1`: The source or payload of the verb, for example, a buffer or path.
 * - `arg2`: The magnitude of the operation, for example, a size or encoding.
 * - `arg3`: The location or a modifier to the operation, for example, an offset or flags.
 * - `arg4`: An auxiliary argument, for example, additional flags or options.
 *
 * It may not always be possible for a verb to follow these conventions, but they should be followed whenever
 * reasonable.
 *
 * @note The kernels internal I/O Request Packet structure uses a similar system but with the kernel equivalents
 * of the arguments, for example, a `file_t*` instead of a `fd_t`.
 *
 * ## Results
 *
 * The result of a SQE is stored in its corresponding CQE using a single 64-bit value. For convenience, the result is
 * stored as a union of various types. Note that this does not actually change the stored value, just how it is
 * interpreted.
 *
 * If a SQE fails, the error code will be stored separately from the result and the result it self may be undefined.
 * Some verbs may allow partial failures in which case the result may still be valid even if an error code is present.
 *
 * @todo Decide if partial failures are a good idea or not.
 *
 * ## Errors
 *
 * The majority of errors are returned in the CQEs, certain errors (such as `ENOMEM`) may be
 * reported directly from the `enter()` call.
 *
 * Error values that may be returned in a CQE include:
 * - `EOK`: Success.
 * - `ECANCELED`: The verb was cancelled.
 * - `ETIMEDOUT`: The verb timed out.
 * - Other values may be returned depending on the verb.
 *
 * ## Verbs
 *
 * Included below is a list of all currently implemented verbs.
 *
 * The arguments of each verb is specified in order as `arg0`, `arg1`, `arg2`, `arg3`, `arg4`.
 *
 * ### `VERB_NOP`
 *
 * A no-operation verb that does nothing but is useful for implementing sleeping.
 *
 * @param arg0 Unused
 * @param arg1 Unused
 * @param arg2 Unused
 * @param arg3 Unused
 * @param arg4 Unused
 * @result None
 *
 * ### `VERB_READ`
 *
 * Reads data from a file descriptor.
 *
 * @param fd The file descriptor to read from.
 * @param buffer The buffer to read the data into.
 * @param count The number of bytes to read.
 * @param offset The offset to read from, or `IO_CUR` to use the current position.
 * @param arg4 Unused
 * @result The number of bytes read.
 *
 * ### `VERB_WRITE`
 *
 * Writes data to a file descriptor.
 *
 * @param fd The file descriptor to write to.
 * @param buffer The buffer to write the data from.
 * @param count The number of bytes to write.
 * @param offset The offset to write to, or `IO_CUR` to use the current position.
 * @param arg4 Unused
 * @result The number of bytes written.
 *
 * ### `VERB_POLL`
 *
 * Polls a file descriptor for events.
 *
 * @param fd The file descriptor to poll.
 * @param events The events to wait for.
 * @param arg2 Unused
 * @param arg3 Unused
 * @param arg4 Unused
 * @result The events that occurred.
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
    ioring_t ring;          ///< The kernel-side ring structure.
    irp_pool_t* irps;       ///< Pool of preallocated IRPs.
    void* userAddr;         ///< Userspace address of the ring.
    void* kernelAddr;       ///< Kernel address of the ring.
    size_t pageAmount;      ///< Amount of pages mapped for the ring.
    wait_queue_t waitQueue; ///< Wait queue for completions.
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