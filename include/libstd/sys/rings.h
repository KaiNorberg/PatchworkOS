#ifndef _SYS_RINGS_H
#define _SYS_RINGS_H 1

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/defs.h>
#include <sys/list.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_internal/MAX_PATH.h"
#include "_internal/clock_t.h"
#include "_internal/errno_t.h"
#include "_internal/fd_t.h"

/**
 * @brief Programmable submission/completion interface.
 * @defgroup libstd_rings Rings
 * @ingroup libstd
 *
 * @todo The rings system is primarily a design document for now as it remains very work in progress and subject to
 * change, currently being mostly unimplemented.
 *
 * Asynchronous operations provide the core of all IO interfaces in PatchworkOS, all implemented in an interface
 * inspired by `io_uring()` from Linux.
 *
 * Synchronous operations are implemented on top of this API in userspace.
 *
 * @see [Wikipedia](https://en.wikipedia.org/wiki/Io_uring) for information about `io_uring`.
 * @see [Manpages](https://man7.org/linux/man-pages/man7/io_uring.7.html) for more information about `io_uring`.
 *
 * ## Registers
 *
 * Operations performed on a ring can load arguments from, and save their results to, seven 64-bit general purpose
 * registers. All registers are stored in the shared area of the rings structure, as such they can be inspected and
 * modified by user space.
 *
 * When a SQE is processed, the kernel will check six register specifiers in the SQE flags, one for each argument and
 * one for the result. Each specifier is stored as three bits, with a `SQE_REG_NONE` value indicating no-op and any
 * other value representing the n-th register. The offset of the specifier specifies its meaning, for example, bits
 * `0-2` specify the register to load into the first argument, bits `3-5` specify the register to load into the second
 * argument, and so on until bits `15-18` which specify the register to save the result into.
 *
 * This system, when combined with `SQE_LINK`, allows for multiple operations to be performed at once, for example, it
 * would be possible to open a file, read from it, seek to a new position, write to it, and finally close the file, with
 * a single `enter()` call.
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
 * @see `sqe_flags_t` for more information about register specifiers and their formatting.
 *
 * ## Syncronization
 *
 * The rings structure is designed to be safe under the assumption that there is a single producer (one user-space
 * thread) and a single consumer (the kernel).
 *
 * If a rings structure needs multiple producers (needs to be accessed by multiple threads) it is the responsibility of
 * the caller to ensure proper synchronization.
 *
 * @note The reason for this limitation is optimization for the common case, as the syncronization logic for multiple
 * producers would add significant overhead.
 *
 * Regarding the rings structure itself, the structure can only be torndown as long as nothing is using it and there are
 * no pending operations.
 *
 * @{
 */

/**
 * @brief Rings operation codes.
 * @enum rings_op_t
 */
typedef enum
{
    RINGS_MIN_OPCODE = 0,
    RINGS_NOP = 0, ///< Never completes, can be used to implement a sleep equivalent by specifying a timeout.
    RINGS_MAX_OPCODE = 1,
} rings_op_t;

/**
 * @brief Maximum number of arguments for a rings operation.
 */
#define SEQ_MAX_ARGS 5

/**
 * @brief Rings register specifiers.
 * @enum seq_regs_t
 *
 * Used in the `sqe_flags_t` enum to specify which registers to load into arguments or save the result into.
 *
 */
typedef enum
{
    SQE_REG0 = 0,         ///< The first register.
    SQE_REG1 = 1,         ///< The second register.
    SQE_REG2 = 2,         ///< The third register.
    SQE_REG3 = 3,         ///< The fourth register.
    SQE_REG4 = 4,         ///< The fifth register.
    SQE_REG5 = 5,         ///< The sixth register.
    SQE_REG6 = 6,         ///< The seventh register.
    SQE_REG_NONE = 7,     ///< No register.
    SEQ_REGS_MAX = 7,     ///< The maximum number of registers.
    SQE_REG_SHIFT = 3,    ///< The bitshift for each register specifier in a `sqe_flags_t`.
    SQE_REG_MASK = 0b111, ///< The bitmask for a register specifier in a `sqe_flags_t`.
} seq_regs_t;

/**
 * @brief Submission queue entry (SQE) flags.
 * @enum sqe_flags_t
 */
typedef enum
{
    SQE_LOAD0 = 0,                         ///< The offset to specify which register to load into the first argument.
    SQE_LOAD1 = SQE_LOAD0 + SQE_REG_SHIFT, ///< The offset to specify which register to load into the second argument.
    SQE_LOAD2 = SQE_LOAD1 + SQE_REG_SHIFT, ///< The offset to specify which register to load into the third argument.
    SQE_LOAD3 = SQE_LOAD2 + SQE_REG_SHIFT, ///< The offset to specify which register to load into the fourth argument.
    SQE_LOAD4 = SQE_LOAD3 + SQE_REG_SHIFT, ///< The offset to specify which register to load into the fifth argument.
    SQE_SAVE = SQE_LOAD4 + SQE_REG_SHIFT,  ///< The offset to specify the register to save the result into.
    SQE_FLAGS_SHIFT = SQE_SAVE + SQE_REG_SHIFT, ///< The bitshift for where bit flags start in a `sqe_flags_t`.
    SQE_LINK = 1 << (SQE_FLAGS_SHIFT), ///< Only process the next SQE if and when this one completes successfully, only
                                       ///< applies within one `enter()` call.
    SQE_RESET = 1 << (SQE_FLAGS_SHIFT + 1), ///< Reset registers before processing this SQE.
} sqe_flags_t;

/**
 * @brief Asynchronous submission queue entry (SQE).
 * @struct sqe_t
 *
 * @warning It is the responsibility of userspace to ensure that any pointers
 * passed to the kernel remain valid until the operation is complete.
 */
typedef struct sqe
{
    rings_op_t opcode; ///< Operation code.
    sqe_flags_t flags; ///< Submission flags.
    clock_t timeout;   ///< Timeout for the operation, `CLOCKS_NEVER` for no timeout.
    void* data;        ///< Private data for the operation, will be returned in the completion entry.
    union {
        struct
        {

        } nop;
        uint64_t args[SEQ_MAX_ARGS];
    };
} sqe_t;

#ifdef static_assert
static_assert(sizeof(sqe_t) == 64, "sqe_t is not 64 bytes");
#endif

/**
 * @brief Macro to create an asynchronous submission queue entry (SQE).
 *
 * @param _opcode Operation code.
 * @param _flags Submission flags.
 * @param _timeout Timeout for the operation, `CLOCKS_NEVER` for no timeout.
 * @param _data Private data for the operation.
 */
#define SQE_CREATE(_opcode, _flags, _timeout, _data) \
    { \
        .opcode = (_opcode), \
        .flags = (_flags), \
        .timeout = (_timeout), \
        .data = (void*)(_data), \
    }

/**
 * @brief Asynchronous completion queue entry (CQE).
 * @struct cqe_t
 */
typedef struct ALIGNED(32) cqe
{
    rings_op_t opcode; ///< Operation code from the submission entry.
    errno_t error;     ///< Error code, if not equal to `EOK` an error occurred.
    void* data;        ///< Private data from the submission entry.
    union {
        uint64_t nop;
        uint64_t _raw;
    };
} cqe_t;

#ifdef static_assert
static_assert(sizeof(cqe_t) == 32, "cqe_t is not 32 bytes");
#endif

/**
 * @brief Rings ID type.
 */
typedef uint64_t rings_id_t;

/**
 * @brief Shared asynchronous rings structure.
 * @struct rings_shared_t
 *
 * Used as the intermediate between userspace and the kernel.
 *
 * @note The structure is aligned in such a way to reduce false sharing.
 *
 */
typedef struct ALIGNED(64) rings_shared
{
    atomic_uint32_t shead;                          ///< Submission head index, updated by the kernel.
    atomic_uint32_t ctail;                          ///< Completion tail index, updated by the kernel.
    atomic_uint32_t stail ALIGNED(64);              ///< Submission tail index, updated by userspace.
    atomic_uint32_t chead;                          ///< Completion head index, updated by userspace.
    atomic_uint64_t regs[SEQ_REGS_MAX] ALIGNED(64); ///< General purpose registers.
} rings_shared_t;

/**
 * @brief Asynchronous rings structure.
 * @struct rings_t
 *
 * The kernel and userspace will have their own instances of this structure.
 */
typedef struct rings
{
    rings_shared_t* shared; ///< Pointer to the shared structure.
    rings_id_t id;          ///< The ID of the rings.
    sqe_t* squeue;          ///< Pointer to the submission queue.
    size_t sentries;        ///< Number of entries in the submission queue.
    size_t smask;           ///< Bitmask for submission queue (sentries - 1).
    cqe_t* cqueue;          ///< Pointer to the completion queue.
    size_t centries;        ///< Number of entries in the completion queue.
    size_t cmask;           ///< Bitmask for completion queue (centries - 1).
} rings_t;

/**
 * @brief Dont wait for any submissions to complete.
 */
#define WAIT_NONE 0x0

/**
 * @brief Wait for at least one submission to complete.
 */
#define WAIT_ONE 0x1

/**
 * @brief System call to initialize the asynchronous rings.
 *
 * This system call will populate the given structure with the necessary pointers and metadata for the submission and
 * completion rings.
 *
 * @param rings Pointer to the structure to populate.
 * @param address Desired address to allocate the rings, or `NULL` to let the kernel choose.
 * @param sentries Number of entires to allocate for the submission queue, must be a power of two.
 * @param centries Number of entries to allocate for the completion queue, must be a power of two.
 * @return On success, the ring ID. On failure, `ERR` and `errno` is set.
 */
rings_id_t setup(rings_t* rings, void* address, size_t sentries, size_t centries);

/**
 * @brief System call to deinitialize the asynchronous rings.
 *
 * @param id The ID of the rings to deinitialize.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t teardown(rings_id_t id);

/**
 * @brief System call to notify the kernel of new submission queue entries (SQEs).
 *
 * @param id The ID of the rings to notify.
 * @param amount The number of SQEs that the kernel should process.
 * @param wait The minimum number of completion queue entries (CQEs) to wait for.
 * @return On success, the number of SQEs successfully processed. On failure, `ERR` and `errno` is set.
 */
uint64_t enter(rings_id_t id, size_t amount, size_t wait);

/**
 * @brief Pushes a submission queue entry (SQE) to the submission queue.
 *
 * After pushing SQEs, `enter()` must be called to notify the kernel of the new entries.
 *
 * @param rings Pointer to the asynchronous rings structure.
 * @param sqe Pointer to the SQE to push.
 * @return `true` if the SQE was pushed, `false` if the submission queue is full.
 */
static inline bool sqe_push(rings_t* rings, sqe_t* sqe)
{
    uint32_t tail = atomic_load_explicit(&rings->shared->stail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&rings->shared->shead, memory_order_acquire);

    if ((tail - head) >= rings->sentries)
    {
        return false;
    }

    rings->squeue[tail & rings->smask] = *sqe;
    atomic_store_explicit(&rings->shared->stail, tail + 1, memory_order_release);

    return true;
}

/**
 * @brief Pops a completion queue entry (CQE) from the completion queue.
 *
 * @param rings Pointer to the asynchronous rings structure.
 * @param cqe Pointer to the CQE to pop.
 * @return `true` if a CQE was popped, `false` if the completion queue is empty.
 */
static inline bool cqe_pop(rings_t* rings, cqe_t* cqe)
{
    uint32_t head = atomic_load_explicit(&rings->shared->chead, memory_order_relaxed);
    uint32_t tail = atomic_load_explicit(&rings->shared->ctail, memory_order_acquire);

    if (head == tail)
    {
        return false;
    }

    *cqe = rings->cqueue[head & rings->cmask];
    atomic_store_explicit(&rings->shared->chead, head + 1, memory_order_release);

    return true;
}

/** @} */

#if defined(__cplusplus)
}
#endif

#endif