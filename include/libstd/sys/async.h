#include <sys/list.h>
#ifndef _SYS_ASYNC_H
#define _SYS_ASYNC_H 1

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/defs.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_internal/MAX_PATH.h"
#include "_internal/clock_t.h"
#include "_internal/errno_t.h"
#include "_internal/fd_t.h"

/**
 * @brief Asynchronous operations.
 * @defgroup libstd_async Async
 * @ingroup libstd
 *
 * Asynchronous operations provide the core of all IO interfaces in PatchworkOS, all implemented in an interface
 * inspired by `io_uring()` from Linux.
 *
 * Synchronous operations are implemented on top of this API in userspace.
 *
 * @todo The async system is not currently implemented, this is more just a draft for now.
 *
 * @see [Wikipedia](https://en.wikipedia.org/wiki/Io_uring) for information about `io_uring`.
 * @see [Manpages](https://man7.org/linux/man-pages/man7/io_uring.7.html) for more information about `io_uring`.
 *
 * @{
 */

/**
 * @brief Asynchronous operation codes.
 * @enum async_op_t
 */
typedef enum
{
    ASYNC_OP_NOP = 0, ///< Never completes, can be used to implement a sleep equivalent.
} async_op_t;

/**
 * @brief Asynchronous sequence flags.
 * @enum async_seq_flags_t
 *
 * Used to modify the behavior of asynchronous operations.
 *
 * @todo Implement `ASYNC_SEQ_LINK`.
 */
typedef enum
{
    ASYNC_SEQ_NONE = 0,
    ASYNC_SEQ_LINK = 1 << 0,      ///< Must be completed before the next SQE in the submission queue is started.
    ASYNC_SEQ_IMMEDIATE = 1 << 1, ///< Fail if the operation cannot be completed immediately.
} async_seq_flags_t;

/**
 * @brief Asynchronous submission queue entry (SQE).
 * @struct async_sqe_t
 *
 * @warning For operations such as `ASYNC_OP_OPEN`, it is the responsibility of userspace to ensure that any pointers
 * passed to the kernel remain valid until the operation is complete.
 */
typedef struct async_sqe
{
    void* data;              ///< Private data for the operation, will be returned in the completion entry.
    async_op_t opcode;       ///< Operation code.
    async_seq_flags_t flags; ///< Sequence flags.
    clock_t timeout;         ///< Timeout for the operation, `CLOCKS_NEVER` for no timeout.
    union {
        struct
        {

        } nop;
        uint64_t _raw[5];
    };
} async_sqe_t;

#ifdef static_assert
static_assert(sizeof(async_sqe_t) == 64, "async_sqe_t is not 64 bytes");
#endif

/**
 * @brief Macro to create an asynchronous submission queue entry (SQE).
 *
 * @param _id Unique identifier for the operation.
 * @param _opcode Operation code.
 * @param _flags Sequence flags.
 * @param _timeout Timeout for the operation, `CLOCKS_NEVER` for no timeout.
 * @param _data Private data for the operation.
 */
#define ASYNC_SQE_CREATE(_id, _opcode, _flags, _timeout, _data) \
    { \
        .data = (_data), \
        .id = (_id), \
        .opcode = (_opcode), \
        .flags = (_flags), \
        .timeout = (_timeout), \
    }

/**
 * @brief Asynchronous completion queue entry (CQE).
 * @struct async_cqe_t
 */
typedef struct ALIGNED(64) async_cqe
{
    void* data;        ///< Private data from the submission entry.
    async_op_t opcode; ///< Operation code from the submission entry.
    errno_t error;     ///< Error code, if not equal to `EOK` an error occurred.
    union {
        size_t read; ///< The number of bytes read from `ASYNC_OP_READ`.
        uint64_t _raw;
    };
} async_cqe_t;

#ifdef static_assert
static_assert(sizeof(async_cqe_t) == 64, "async_cqe_t is not 64 bytes");
#endif
/**
 * @brief Shared asynchronous rings structure.
 * @struct async_shared_t
 *
 * Used as the intermediate between userspace and the kernel.
 *
 */
typedef struct ALIGNED(64) async_shared
{
    atomic_uint32_t shead; ///< Submission head index, updated by the kernel.
    atomic_uint32_t ctail; ///< Completion tail index, updated by the kernel.
    uint8_t _padding[64 -
        sizeof(atomic_uint32_t) * 2]; ///< Padding to prevent false sharing between user space and the kernel.
    atomic_uint32_t stail;            ///< Submission tail index, updated by userspace.
    atomic_uint32_t chead;            ///< Completion head index, updated by userspace.
} async_shared_t;

/**
 * @brief Asynchronous rings structure.
 * @struct async_rings_t
 *
 * The kernel and userspace will have their own instances of this structure.
 */
typedef struct async_rings
{
    async_shared_t* shared; ///< Pointer to the shared structure.
    async_sqe_t* squeue;    ///< Pointer to the submission queue.
    size_t sentries;        ///< Number of entries in the submission queue.
    size_t smask;           ///< Bitmask for submission queue (sentries - 1).
    async_cqe_t* cqueue;    ///< Pointer to the completion queue.
    size_t centries;        ///< Number of entries in the completion queue.
    size_t cmask;           ///< Bitmask for completion queue (centries - 1).
} async_rings_t;

/**
 * @brief Dont wait for any submissions to complete.
 */
#define ASYNC_WAIT_NONE 0x0

/**
 * @brief Wait for at least one submission to complete.
 */
#define ASYNC_WAIT_ONE 0x1

/**
 * @brief Wait for all submissions to complete.
 */
#define ASYNC_WAIT_ALL SIZE_MAX

/**
 * @brief System call to initialize the asynchronous rings.
 *
 * This system call will populate the given structure with the necessary pointers and metadata for the submission and
 * completion rings.
 *
 * @note Since each process can only have one rings set, the `async_deinit()` system call must be used before calling
 * this function again.
 *
 * @param rings Pointer to the structure to populate.
 * @param address Desired address to allocate the rings, or `NULL` to let the kernel choose.
 * @param sentries Number of entires to allocate for the submission queue, must be a power of two.
 * @param centries Number of entries to allocate for the completion queue, must be a power of two.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t async_init(async_rings_t* rings, void* address, size_t sentries, size_t centries);

/**
 * @brief System call to deinitialize the asynchronous rings.
 *
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t async_deinit(void);

/**
 * @brief System call to notify the kernel of new submission queue entries (SQEs).
 *
 * @param amount The number of SQEs that the kernel should process.
 * @param wait The minimum number of completion queue entries (CQEs) to wait for.
 * @return On success, the number of SQEs successfully processed. On failure, `ERR` and `errno` is set.
 */
uint64_t async_notify(size_t amount, size_t wait);

/**
 * @brief Pushes a submission queue entry (SQE) to the submission queue.
 *
 * After pushing SQEs, `async_notify()` must be called to notify the kernel of the new entries.
 *
 * @param rings Pointer to the asynchronous rings structure.
 * @param sqe Pointer to the SQE to push.
 * @return `true` if the SQE was pushed, `false` if the submission queue is full.
 */
static inline bool async_push_sqe(async_rings_t* rings, async_sqe_t* sqe)
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
static inline bool async_pop_cqe(async_rings_t* rings, async_cqe_t* cqe)
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