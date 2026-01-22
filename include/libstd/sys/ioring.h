#ifndef _SYS_IORING_H
#define _SYS_IORING_H 1

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
#include "_internal/ssize_t.h"

/**
 * @addtogroup kernel_io
 * @{
 */

typedef uint64_t whence_t;     ///< Seek origin type.
#define IO_SET ((ssize_t) - 3) ///< Use the start of the file.
#define IO_END ((ssize_t) - 2) ///< Use the end of the file.
#define IO_CUR ((ssize_t) - 1) ///< Use the current file offset.

typedef uint64_t events_t;   ///< Poll events type.
#define IO_READABLE (1 << 0) ///< File descriptor is ready to read.
#define IO_WRITABLE (1 << 1) ///< File descriptor is ready to write
#define IO_ERROR (1 << 2)    ///< File descriptor caused an error.
#define IO_CLOSED (1 << 3)   ///< File descriptor is closed.
#define IO_INVALID (1 << 4)  ///< Invalid file descriptor.

typedef uint32_t verb_t; ///< Verb type.
#define VERB_NOP 0       ///< No-op verb.
#define VERB_READ 1      ///< Read verb.
#define VERB_WRITE 2     ///< Write verb.
#define VERB_POLL 3      ///< Poll verb.
#define VERB_MAX 4 ///< The maximum number of verbs.

typedef uint32_t sqe_flags_t; ///< Submission queue entry (SQE) flags.

#define SQE_REG0 (0)         ///< The first register.
#define SQE_REG1 (1)         ///< The second register.
#define SQE_REG2 (2)         ///< The third register.
#define SQE_REG3 (3)         ///< The fourth register.
#define SQE_REG4 (4)         ///< The fifth register.
#define SQE_REG5 (5)         ///< The sixth register.
#define SQE_REG6 (6)         ///< The seventh register.
#define SQE_REG_NONE (7)     ///< No register.
#define SQE_REGS_MAX (7)     ///< The maximum number of registers.
#define SQE_REG_SHIFT (3)    ///< The bitshift for each register specifier in a `sqe_flags_t`.
#define SQE_REG_MASK (0b111) ///< The bitmask for a register specifier in a `sqe_flags_t`.

#define SQE_LOAD0 (0)                         ///< The offset to specify the register to load into the first argument.
#define SQE_LOAD1 (SQE_LOAD0 + SQE_REG_SHIFT) ///< The offset to specify the register to load into the second argument.
#define SQE_LOAD2 (SQE_LOAD1 + SQE_REG_SHIFT) ///< The offset to specify the register to load into the third argument.
#define SQE_LOAD3 (SQE_LOAD2 + SQE_REG_SHIFT) ///< The offset to specify the register to load into the fourth argument.
#define SQE_LOAD4 (SQE_LOAD3 + SQE_REG_SHIFT) ///< The offset to specify the register to load into the fifth argument.
#define SQE_SAVE (SQE_LOAD4 + SQE_REG_SHIFT)  ///< The offset to specify the register to save the result into.

#define _SQE_FLAGS (SQE_SAVE + SQE_REG_SHIFT) ///< The bitshift for where bit flags start in a `sqe_flags_t`.

#ifdef _KERNEL_
/**
 * The operation was created by the kernel, used internally by the kernel.
 */
#define SQE_KERNEL (1 << (_SQE_FLAGS))
#endif

/**
 * Only process the next SQE when this one completes successfully) only applies within one `enter()` call.
 */
#define SQE_LINK (1 << (_SQE_FLAGS + 2))
/**
 * Like `SQE_LINK` but will process the next SQE even if this one fails.
 */
#define SQE_HARDLINK (1 << (_SQE_FLAGS + 3))

/**
 * @brief Asynchronous submission queue entry (SQE).
 * @struct sqe_t
 *
 * @warning It is the responsibility of userspace to ensure that any pointers
 * passed to the kernel remain valid until the operation is complete.
 *
 * @see kernel_io for more information for each possible verb.
 */
typedef struct sqe
{
    verb_t verb;       ///< Verb specifying the action to perform.
    sqe_flags_t flags; ///< Submission flags.
    clock_t timeout;   ///< Timeout for the operation, `CLOCKS_NEVER` for no timeout.
    void* data;        ///< Private data for the operation, will be returned in the completion entry.
    union
    {
        uint64_t arg0; 
        fd_t fd;
    };
    union
    {
        uint64_t arg1;
        void* buffer;
        events_t events;
    };
    union
    {
        uint64_t arg2;
        size_t count;
    };
    union
    {
        uint64_t arg3;
        ssize_t offset;
    };
    union
    {
        uint64_t arg4;
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
 * @see kernel_io for more information on the possible operations.
 */
typedef struct cqe
{
    verb_t verb;   ///< Verb specifying the action that was performed.
    errno_t error; ///< Error code, if not equal to `EOK` an error occurred.
    void* data;    ///< Private data from the submission entry.
    union {
        fd_t fd;
        size_t count;
        void* ptr;
        events_t events;
        uint64_t _result;
    };
    uint64_t _padding[1];
} cqe_t;

#ifdef static_assert
static_assert(sizeof(cqe_t) == 32, "cqe_t is not 32 bytes");
#endif

/**
 * @brief Shared ring control structure.
 * @struct ioring_ctrl_t
 *
 * Used as the intermediate between userspace and the kernel.
 *
 * @note The structure is aligned in such a way to reduce false sharing.
 *
 */
typedef struct ALIGNED(64) ioring_ctrl
{
    atomic_uint32_t shead; ///< Submission head index, updated by the kernel.
    atomic_uint32_t ctail; ///< Completion tail index, updated by the kernel.
    uint8_t _padding0[64 - sizeof(atomic_uint32_t) * 2];
    atomic_uint32_t stail; ///< Submission tail index, updated by userspace.
    atomic_uint32_t chead; ///< Completion head index, updated by userspace.
    uint8_t _padding1[64 - sizeof(atomic_uint32_t) * 2];
    atomic_uint64_t regs[SQE_REGS_MAX] ALIGNED(64); ///< General purpose registers.
    uint8_t _reserved[8];
} ioring_ctrl_t;

/**
 * @}
 * @brief Programmable submission/completion interface.
 * @defgroup libstd_sys_ioring User-side I/O Ring Interface
 * @ingroup libstd
 *
 * The ring interface acts as the interface for all asynchronous operations in the kernel.
 *
 * @see kernel_io for more information about the I/O ring system.
 *
 * @{
 */

typedef uint64_t io_id_t; ///< I/O ring ID type.

/**
 * @brief User I/O ring structure.
 * @struct ioring_t
 *
 * The kernel and userspace will have their own instances of this structure.
 */
typedef struct ioring
{
    ioring_ctrl_t* ctrl; ///< Pointer to the shared control structure.
    io_id_t id;          ///< The ID of the ring.
    sqe_t* squeue;       ///< Pointer to the submission queue.
    size_t sentries;     ///< Number of entries in the submission queue.
    size_t smask;        ///< Bitmask for submission queue (sentries - 1).
    cqe_t* cqueue;       ///< Pointer to the completion queue.
    size_t centries;     ///< Number of entries in the completion queue.
    size_t cmask;        ///< Bitmask for completion queue (centries - 1).
} ioring_t;

/**
 * @brief System call to initialize the I/O ring.
 *
 * This system call will populate the given structure with the necessary pointers and metadata for the submission and
 * completion ring.
 *
 * @param ring Pointer to the ring structure to populate.
 * @param address Desired address to allocate the ring, or `NULL` to let the kernel choose.
 * @param sentries Number of entires to allocate for the submission queue, must be a power of two.
 * @param centries Number of entries to allocate for the completion queue, must be a power of two.
 * @return On success, the ID of the new I/O ring. On failure, `ERR` and `errno` is set.
 */
io_id_t setup(ioring_t* ring, void* address, size_t sentries, size_t centries);

/**
 * @brief System call to deinitialize the I/O ring.
 *
 * @param id The ID of the I/O ring to teardown.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t teardown(io_id_t id);

/**
 * @brief System call to notify the kernel of new submission queue entries (SQEs).
 *
 * @param id The ID of the I/O ring to notify.
 * @param amount The number of SQEs that the kernel should process.
 * @param wait The minimum number of completion queue entries (CQEs) to wait for.
 * @return On success, the number of SQEs successfully processed. On failure, `ERR` and `errno` is set.
 */
uint64_t enter(io_id_t id, size_t amount, size_t wait);

/**
 * @brief Pushes a submission queue entry (SQE) to the submission queue.
 *
 * After pushing SQEs, `enter()` must be called to notify the kernel of the new entries.
 *
 * @param ring Pointer to the I/O ring structure.
 * @param sqe Pointer to the SQE to push.
 * @return `true` if the SQE was pushed, `false` if the submission queue is full.
 */
static inline bool sqe_push(ioring_t* ring, sqe_t* sqe)
{
    uint32_t tail = atomic_load_explicit(&ring->ctrl->stail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&ring->ctrl->shead, memory_order_acquire);

    if ((tail - head) >= ring->sentries)
    {
        return false;
    }

    ring->squeue[tail & ring->smask] = *sqe;
    atomic_store_explicit(&ring->ctrl->stail, tail + 1, memory_order_release);

    return true;
}

/**
 * @brief Pops a completion queue entry (CQE) from the completion queue.
 *
 * @param ring Pointer to the I/O ring structure.
 * @param cqe Pointer to the CQE to pop.
 * @return `true` if a CQE was popped, `false` if the completion queue is empty.
 */
static inline bool cqe_pop(ioring_t* ring, cqe_t* cqe)
{
    uint32_t head = atomic_load_explicit(&ring->ctrl->chead, memory_order_relaxed);
    uint32_t tail = atomic_load_explicit(&ring->ctrl->ctail, memory_order_acquire);

    if (head == tail)
    {
        return false;
    }

    *cqe = ring->cqueue[head & ring->cmask];
    atomic_store_explicit(&ring->ctrl->chead, head + 1, memory_order_release);

    return true;
}

/** @} */

#if defined(__cplusplus)
}
#endif

#endif