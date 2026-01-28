#ifndef _SYS_IORING_H
#define _SYS_IORING_H 1

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/defs.h>
#include <sys/list.h>
#include <sys/status.h>
#include <sys/syscall.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libstd/MAX_NAME.h"
#include "_libstd/MAX_PATH.h"
#include "_libstd/clock_t.h"
#include "_libstd/errno_t.h"
#include "_libstd/fd_t.h"
#include "_libstd/ssize_t.h"

/**
 * @brief Programmable submission/completion interface.
 * @defgroup libstd_sys_ioring I/O Ring ABI
 * @ingroup libstd
 *
 * The ring interface acts as the interface for all asynchronous operations in the kernel.
 *
 * @see kernel_io_ioring for more information about I/O rings.
 *
 * @{
 */

#define IO_OFF_CUR ((ssize_t) - 1) ///< Use the current file offset.

typedef uint64_t io_whence_t; ///< Seek origin type.
#define IO_SEEK_SET (1)       ///< Use the start of the file.
#define IO_SEEK_END (2)       ///< Use the end of the file.
#define IO_SEEK_CUR (3)       ///< Use the current file offset.

typedef uint64_t io_events_t;  ///< Poll events type.
#define IO_POLL_READ (1 << 0)  ///< File descriptor is ready to read.
#define IO_POLL_WRITE (1 << 1) ///< File descriptor is ready to write
#define IO_POLL_ERROR (1 << 2) ///< File descriptor caused an error.
#define IO_POLL_HUP (1 << 3)   ///< File descriptor is closed.
#define IO_POLL_NVAL (1 << 4)  ///< Invalid file descriptor.

typedef uint32_t io_op_t; ///< I/O operation code type.

/**
 * @brief No-op operation.
 *
 * @see sqe_prep_nop
 */
#define IO_OP_NOP 0

/**
 * @brief Cancel operation.
 * @see sqe_prep_cancel
 */
#define IO_OP_CANCEL 1

/**
 * @brief Read operation.
 *
 * @see sqe_prep_read
 */
#define IO_OP_READ 2

/**
 * @brief Write operation.
 *
 * @see sqe_prep_write
 */
#define IO_OP_WRITE 3

/**
 * @brief Poll operation.
 *
 * @see sqe_prep_poll
 */
#define IO_OP_POLL 4

#define IO_OP_MAX 5 ///< The maximum number of operation.

typedef uint64_t io_cancel_t;  ///< Cancel operation flags.
#define IO_CANCEL_ALL (1 << 0) ///< Cancel all matching requests.
#define IO_CANCEL_ANY (1 << 1) ///< Match any user data.

typedef uint32_t sqe_flags_t; ///< Submission queue entry (SQE) flags.
#define SQE_REG_NONE (0)      ///< No register.
#define SQE_REG0 (1)          ///< The first register.
#define SQE_REG1 (2)          ///< The second register.
#define SQE_REG2 (3)          ///< The third register.
#define SQE_REG3 (4)          ///< The fourth register.
#define SQE_REG4 (5)          ///< The fifth register.
#define SQE_REG5 (6)          ///< The sixth register.
#define SQE_REG6 (7)          ///< The seventh register.
#define SQE_REGS_MAX (7)      ///< The maximum number of registers.
#define SQE_REG_SHIFT (3)     ///< The bitshift for each register specifier in a `sqe_flags_t`.
#define SQE_REG_MASK (0b111)  ///< The bitmask for a register specifier in a `sqe_flags_t`.

#define SQE_LOAD0 (0)                         ///< The offset to specify the register to load into the first argument.
#define SQE_LOAD1 (SQE_LOAD0 + SQE_REG_SHIFT) ///< The offset to specify the register to load into the second argument.
#define SQE_LOAD2 (SQE_LOAD1 + SQE_REG_SHIFT) ///< The offset to specify the register to load into the third argument.
#define SQE_LOAD3 (SQE_LOAD2 + SQE_REG_SHIFT) ///< The offset to specify the register to load into the fourth argument.
#define SQE_LOAD4 (SQE_LOAD3 + SQE_REG_SHIFT) ///< The offset to specify the register to load into the fifth argument.
#define SQE_SAVE (SQE_LOAD4 + SQE_REG_SHIFT)  ///< The offset to specify the register to save the result into.

#define _SQE_FLAGS (SQE_SAVE + SQE_REG_SHIFT) ///< The bitshift for where bit flags start in a `sqe_flags_t`.

#define SQE_NORMAL 0 ///< Default behaviour flags.

/**
 * Only process the next SQE when this one completes successfully) only applies within one `enter()` call.
 */
#define SQE_LINK (1 << (_SQE_FLAGS))
/**
 * Like `SQE_LINK` but will process the next SQE even if this one fails.
 */
#define SQE_HARDLINK (1 << (_SQE_FLAGS + 1))

/**
 * @brief Asynchronous submission queue entry (SQE).
 * @struct sqe_t
 *
 * @warning It is the responsibility of userspace to ensure that any pointers
 * passed to the kernel remain valid until the operation is complete.
 *
 * @see kernel_io for more information for each possible operation.
 */
typedef struct sqe
{
    clock_t timeout;   ///< Timeout for the operation, `CLOCKS_NEVER` for no timeout.
    uintptr_t data;    ///< Private data for the operation, will be returned in the completion entry.
    io_op_t op;        ///< The operation to perform.
    sqe_flags_t flags; ///< Submission flags.
    union {
        uint64_t arg0;
        fd_t fd;
        uintptr_t target;
    };
    union {
        uint64_t arg1;
        void* buffer;
        io_events_t events;
        io_cancel_t cancel;
    };
    union {
        uint64_t arg2;
        size_t count;
    };
    union {
        uint64_t arg3;
        ssize_t offset;
    };
    union {
        uint64_t arg4;
    };
} sqe_t;

#ifdef static_assert
static_assert(sizeof(sqe_t) == 64, "sqe_t is not 64 bytes");
#endif

/**
 * @brief Macro to create an asynchronous submission queue entry (SQE).
 *
 * @param _op The operation to perform.
 * @param _flags Submission flags.
 * @param _timeout Timeout for the operation, `CLOCKS_NEVER` for no timeout.
 * @param _data Private data for the operation.
 */
#define SQE_CREATE(_op, _flags, _timeout, _data) \
    (sqe_t) \
    { \
        .op = (_op), .flags = (_flags), .timeout = (_timeout), .data = (_data), \
    }

/**
 * @brief Asynchronous completion queue entry (CQE).
 * @struct cqe_t
 *
 * @see kernel_io for more information on the possible operations.
 */
typedef struct cqe
{
    io_op_t op;      ///< The operation that was performed.
    status_t status; ///< The status of the operation.
    uintptr_t data;  ///< Private data from the submission entry.
    union {
        fd_t fd;
        size_t count;
        void* ptr;
        io_events_t events;
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

typedef uint64_t ioring_id_t; ///< I/O ring ID type.

/**
 * @brief User I/O ring structure.
 * @struct ioring_t
 *
 * The kernel and userspace will have their own instances of this structure.
 */
typedef struct ioring
{
    ioring_ctrl_t* ctrl; ///< Pointer to the shared control structure.
    ioring_id_t id;      ///< The ID of the ring.
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
 * @return An appropriate status value.
 */
static inline status_t ioring_setup(ioring_t* ring, void* address, size_t sentries, size_t centries)
{
    return syscall4(SYS_IORING_SETUP, NULL, (uintptr_t)ring, (uintptr_t)address, sentries, centries);
}

/**
 * @brief System call to deinitialize the I/O ring.
 *
 * @param ring The ring to deinitialize.
 * @return An appropriate status value.
 */
static inline status_t ioring_teardown(ioring_t* ring)
{
    return syscall1(SYS_IORING_TEARDOWN, NULL, ring->id);
}

/**
 * @brief System call to notify the kernel of new submission queue entries (SQEs).
 *
 * @param ring The ring to enter.
 * @param amount The number of SQEs that the kernel should process.
 * @param wait The minimum number of completion queue entries (CQEs) to wait for.
 * @param processed Output pointer for the number of SQEs processed.
 * @return An appropriate status value.
 */
static inline status_t ioring_enter(ioring_t* ring, size_t amount, size_t wait, size_t* processed)
{
    return syscall3(SYS_IORING_ENTER, processed, ring->id, (uintptr_t)amount, (uintptr_t)wait);
}

/**
 * @brief Retrieve the next available submission queue entry (SQE) from the ring.
 *
 * @param ring The I/O ring.
 * @return On success, a pointer to the next available SQE. If the ring is full, `NULL`.
 */
static inline sqe_t* sqe_get(ioring_t* ring)
{
    uint32_t tail = atomic_load_explicit(&ring->ctrl->stail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&ring->ctrl->shead, memory_order_acquire);

    if ((tail - head) >= ring->sentries)
    {
        return NULL;
    }

    return &ring->squeue[tail & ring->smask];
}

/**
 * @brief Commit the next submission queue entry (SQE) to the ring.
 *
 * @param ring The I/O ring.
 */
static inline void sqe_put(ioring_t* ring)
{
    uint32_t tail = atomic_load_explicit(&ring->ctrl->stail, memory_order_relaxed);
    atomic_store_explicit(&ring->ctrl->stail, tail + 1, memory_order_release);
}

/**
 * @brief Prepare a no-op submission queue entry (SQE).
 *
 * @param sqe The SQE to prepare.
 * @param flags Submission flags.
 * @param timeout Timeout for the operation, `CLOCKS_NEVER` for no timeout.
 * @param data Private data for the operation.
 *
 * @see `IO_OP_NOP`
 */
static inline void sqe_prep_nop(sqe_t* sqe, sqe_flags_t flags, clock_t timeout, uintptr_t data)
{
    *sqe = SQE_CREATE(IO_OP_NOP, flags, timeout, data);
}

/**
 * @brief Prepare a read submission queue entry (SQE).
 *
 * @param sqe The SQE to prepare.
 * @param flags Submission flags.
 * @param timeout Timeout for the operation, `CLOCKS_NEVER` for no timeout.
 * @param data Private data for the operation.
 * @param fd The file descriptor to read from (arg0).
 * @param buffer The buffer to read into (arg1).
 * @param count The number of bytes to read (arg2).
 * @param offset The offset to read from, or `IO_OFF_CUR` to use the current position (arg3).
 *
 * @see `IO_OP_READ`
 */
static inline void sqe_prep_read(sqe_t* sqe, sqe_flags_t flags, clock_t timeout, uintptr_t data, fd_t fd, void* buffer,
    size_t count, ssize_t offset)
{
    *sqe = SQE_CREATE(IO_OP_READ, flags, timeout, data);
    sqe->fd = fd;
    sqe->buffer = buffer;
    sqe->count = count;
    sqe->offset = offset;
}

/**
 * @brief Prepare a write submission queue entry (SQE).
 *
 * @param sqe The SQE to prepare.
 * @param flags Submission flags.
 * @param timeout Timeout for the operation, `CLOCKS_NEVER` for no timeout.
 * @param data Private data for the operation.
 * @param fd The file descriptor to write to (arg0).
 * @param buffer The buffer to write from (arg1).
 * @param count The number of bytes to write (arg2).
 * @param offset The offset to write to, or `IO_OFF_CUR` to use the current position (arg3).
 *
 * @see `IO_OP_WRITE`
 */
static inline void sqe_prep_write(sqe_t* sqe, sqe_flags_t flags, clock_t timeout, uintptr_t data, fd_t fd,
    const void* buffer, size_t count, ssize_t offset)
{
    *sqe = SQE_CREATE(IO_OP_WRITE, flags, timeout, data);
    sqe->fd = fd;
    sqe->buffer = (void*)buffer;
    sqe->count = count;
    sqe->offset = offset;
}

/**
 * @brief Prepare a poll submission queue entry (SQE).
 *
 * @param sqe The SQE to prepare.
 * @param flags Submission flags.
 * @param timeout Timeout for the operation, `CLOCKS_NEVER` for no timeout.
 * @param data Private data for the operation.
 * @param fd The file descriptor to poll (arg0).
 * @param events The events to wait for (arg1).
 *
 * @see `IO_OP_POLL`
 */
static inline void sqe_prep_poll(sqe_t* sqe, sqe_flags_t flags, clock_t timeout, uintptr_t data, fd_t fd,
    io_events_t events)
{
    *sqe = SQE_CREATE(IO_OP_POLL, flags, timeout, data);
    sqe->fd = fd;
    sqe->events = events;
}

/**
 * @brief Prepare a cancel submission queue entry (SQE).
 *
 * @param sqe The SQE to prepare.
 * @param flags Submission flags.
 * @param timeout Timeout for the operation, `CLOCKS_NEVER` for no timeout.
 * @param data Private data for the operation.
 * @param target The user data of the operation(s) to cancel (arg0).
 * @param cancel Cancellation flags (arg1).
 *
 * @see `IO_OP_CANCEL`
 */
static inline void sqe_prep_cancel(sqe_t* sqe, sqe_flags_t flags, clock_t timeout, uintptr_t data, uintptr_t target,
    io_cancel_t cancel)
{
    *sqe = SQE_CREATE(IO_OP_CANCEL, flags, timeout, data);
    sqe->target = target;
    sqe->cancel = cancel;
}

/**
 * @brief Retrieve the next available completion queue entry (CQE) from the ring.
 *
 * @param ring The I/O ring.
 * @return On success, a pointer to the next available CQE. If the ring is empty, `NULL`.
 */
static inline cqe_t* cqe_get(ioring_t* ring)
{
    uint32_t head = atomic_load_explicit(&ring->ctrl->chead, memory_order_relaxed);
    uint32_t tail = atomic_load_explicit(&ring->ctrl->ctail, memory_order_acquire);

    if (head == tail)
    {
        return NULL;
    }

    return &ring->cqueue[head & ring->cmask];
}

/**
 * @brief Commit the next completion queue entry (CQE) to the ring.
 *
 * @param ring The I/O ring.
 */
static inline void cqe_put(ioring_t* ring)
{
    uint32_t head = atomic_load_explicit(&ring->ctrl->chead, memory_order_relaxed);
    atomic_store_explicit(&ring->ctrl->chead, head + 1, memory_order_release);
}

/** @} */

#if defined(__cplusplus)
}
#endif

#endif