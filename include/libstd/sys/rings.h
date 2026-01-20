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

#include "_internal/MAX_NAME.h"
#include "_internal/MAX_PATH.h"
#include "_internal/clock_t.h"
#include "_internal/errno_t.h"
#include "_internal/fd_t.h"

/**
 * @addtogroup kernel_sync_async
 * @{
 */

typedef uint32_t verb_t; ///< Verb type.

#define VERB_NOP 0  ///< No-op verb.
#define VERB_OPEN 1 ///< Open file verb.
#define VERB_MAX 1  ///< Maximum verb.

#define SEQ_MAX_ARGS 5 ///< Maximum number of arguments for a rings operation.

typedef uint32_t sqe_flags_t; ///< Submission queue entry (SQE) flags.

#define SQE_REG0 (0)         ///< The first register.
#define SQE_REG1 (1)         ///< The second register.
#define SQE_REG2 (2)         ///< The third register.
#define SQE_REG3 (3)         ///< The fourth register.
#define SQE_REG4 (4)         ///< The fifth register.
#define SQE_REG5 (5)         ///< The sixth register.
#define SQE_REG6 (6)         ///< The seventh register.
#define SQE_REG_NONE (7)     ///< No register.
#define SEQ_REGS_MAX (7)     ///< The maximum number of registers.
#define SQE_REG_SHIFT (3)    ///< The bitshift for each register specifier in a `sqe_flags_t`.
#define SQE_REG_MASK (0b111) ///< The bitmask for a register specifier in a `sqe_flags_t`.

#define SQE_LOAD0 (0) ///< The offset to specify which register to load into the first argument.
#define SQE_LOAD1 \
    (SQE_LOAD0 + SQE_REG_SHIFT) ///< The offset to specify which register to load into the second argument.
#define SQE_LOAD2 (SQE_LOAD1 + SQE_REG_SHIFT) ///< The offset to specify which register to load into the third argument.
#define SQE_LOAD3 \
    (SQE_LOAD2 + SQE_REG_SHIFT) ///< The offset to specify which register to load into the fourth argument.
#define SQE_LOAD4 (SQE_LOAD3 + SQE_REG_SHIFT) ///< The offset to specify which register to load into the fifth argument.
#define SQE_SAVE (SQE_LOAD4 + SQE_REG_SHIFT)  ///< The offset to specify the register to save the result into.
#define SQE_FLAGS_SHIFT (SQE_SAVE + SQE_REG_SHIFT) ///< The bitshift for where bit flags start in a `sqe_flags_t`.
#define SQE_LINK \
    (1 << (SQE_FLAGS_SHIFT)) ///< Only process the next SQE when this one completes successfully) only
                             /// applies within one `enter()` call.
#define SQE_HARDLINK \
    (1 << (SQE_FLAGS_SHIFT + 1)) ///< Like `SQE_LINK`) but will process the next SQE even if this one fails.

/**
 * @brief Asynchronous submission queue entry (SQE).
 * @struct sqe_t
 *
 * @warning It is the responsibility of userspace to ensure that any pointers
 * passed to the kernel remain valid until the operation is complete.
 *
 * @see kernel_sync_async for more information on the possible operations.
 */
typedef struct sqe
{
    verb_t verb;       ///< Verb specifying the action to perform.
    sqe_flags_t flags; ///< Submission flags.
    clock_t timeout;   ///< Timeout for the operation, `CLOCKS_NEVER` for no timeout.
    void* data;        ///< Private data for the operation, will be returned in the completion entry.
    union {
        struct
        {
            uint64_t none;
        } nop;
        struct
        {
            fd_t from;
            char* path;
            size_t length;
        } open;
        uint64_t _args[SEQ_MAX_ARGS];
    };
} sqe_t;

#ifdef static_assert
static_assert(sizeof(sqe_t) == 64, "sqe_t is not 64 bytes");
#endif

/**
 * @brief Macro to create an asynchronous submission queue entry (SQE).
 *
 * @param _verb Operation verb.
 * @param _flags Submission flags.
 * @param _timeout Timeout for the operation, `CLOCKS_NEVER` for no timeout.
 * @param _data Private data for the operation.
 */
#define SQE_CREATE(_verb, _flags, _timeout, _data) \
    { \
        .verb = (_verb), \
        .flags = (_flags), \
        .timeout = (_timeout), \
        .data = (void*)(_data), \
    }

/**
 * @brief Asynchronous completion queue entry (CQE).
 * @struct cqe_t
 *
 * @see kernel_sync_async for more information on the possible operations.
 */
typedef struct ALIGNED(32) cqe
{
    verb_t verb;   ///< Verb specifying the action that was performed.
    errno_t error; ///< Error code, if not equal to `EOK` an error occurred.
    void* data;    ///< Private data from the submission entry.
    union {
        uint64_t nop;
        fd_t open;
        uint64_t _result;
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
 * @}
 * @brief User-side asynchronous rings interface.
 * @defgroup libstd_sys_rings User Asynchronous Rings
 * @ingroup libstd
 *
 * The rings interface acts as the interface for all asynchronous operations in the kernel.
 *
 * @see kernel_sync_async for more information about the asynchronous rings system.
 *
 * @{
 */

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