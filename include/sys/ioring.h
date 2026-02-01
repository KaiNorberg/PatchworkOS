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
 * @todo Write I/O Ring user-side interface documentation.
 *
 * @see kernel_io_ioring for more information about I/O rings.
 *
 * @{
 */

#define IOOFF_CUR (__SIZE_MAX__) ///< Use the current file offset.

typedef uint64_t iowhence_t; ///< Seek origin type.
#define IOSEEK_SET (1)       ///< Use the start of the file.
#define IOSEEK_END (2)       ///< Use the end of the file.
#define IOSEEK_CUR (3)       ///< Use the current file offset.

typedef uint64_t ioevents_t;  ///< Poll events type.
#define IOPOLL_READ (1 << 0)  ///< File descriptor is ready to be read from.
#define IOPOLL_WRITE (1 << 1) ///< File descriptor is ready to be written to.
#define IOPOLL_ERROR (1 << 2) ///< File descriptor caused an error.
#define IOPOLL_HUP (1 << 3)   ///< File descriptor is closed.
#define IOPOLL_NVAL (1 << 4)  ///< Invalid file descriptor.

typedef uint32_t ioop_t; ///< I/O operation code type.

/**
 * @brief No-op operation.
 *
 * Arguments:
 * - None.
 *
 * Result:
 * - `0`
 */
#define IOOP_NOP 0

/**
 * @brief Cancel operation.
 *
 * Arguments:
 * - arg0: `uintptr_t target` - The user data of the operation(s) to cancel.
 * - arg1: `iocancel_t flags` - Cancellation flags.
 *
 * Result:
 * - `0`
 */
#define IOOP_CANCEL 1

/**
 * @brief Read operation.
 *
 * Arguments:
 * - arg0: `fd_t fd` - The file descriptor to read from.
 * - arg1: `void* buffer` - The buffer to read into.
 * - arg2: `size_t count` - The number of bytes to read.
 * - arg3: `ssize_t offset` - The offset to read from, or `IOOFF_CUR`.
 *
 * Result:
 * - `size_t` - The number of bytes read.
 */
#define IOOP_READ 2

/**
 * @brief Write operation.
 *
 * Arguments:
 * - arg0: `fd_t fd` - The file descriptor to write to.
 * - arg1: `void* buffer` - The buffer to write from.
 * - arg2: `size_t count` - The number of bytes to write.
 * - arg3: `ssize_t offset` - The offset to write to, or `IOOFF_CUR`.
 *
 * Result:
 * - `size_t` - The number of bytes written.
 */
#define IOOP_WRITE 3

/**
 * @brief Poll operation.
 *
 * Arguments:
 * - arg0: `fd_t fd` - The file descriptor to poll.
 * - arg1: `ioevents_t events` - The events to wait for.
 *
 * Result:
 * - `ioevents_t` - The events that occurred.
 */
#define IOOP_POLL 4

#define IOOP_MAX 5 ///< The maximum number of operation.

typedef uint64_t iocancel_t;  ///< Cancel operation flags.
#define IOCANCEL_ALL (1 << 0) ///< Cancel all matching requests.
#define IOCANCEL_ANY (1 << 1) ///< Match any user data.

typedef uint32_t iosqe_flags_t; ///< Submission queue entry (SQE) flags.
#define IOSQE_NORMAL 0          ///< Default behaviour flags.

#define IOSQE_REG_NONE (0)     ///< No register.
#define IOSQE_REG0 (1)         ///< The first register.
#define IOSQE_REG1 (2)         ///< The second register.
#define IOSQE_REG2 (3)         ///< The third register.
#define IOSQE_REG3 (4)         ///< The fourth register.
#define IOSQE_REG4 (5)         ///< The fifth register.
#define IOSQE_REG5 (6)         ///< The sixth register.
#define IOSQE_REG6 (7)         ///< The seventh register.
#define IOSQE_REGS_MAX (7)     ///< The maximum number of registers.
#define IOSQE_REG_SHIFT (3)    ///< The bitshift for each register specifier in a `iosqe_flags_t`.
#define IOSQE_REG_MASK (0b111) ///< The bitmask for a register specifier in a `iosqe_flags_t`.

#define IOSQE_LOAD0 (0) ///< The offset to specify the register to load into the first argument.
#define IOSQE_LOAD1 \
    (IOSQE_LOAD0 + IOSQE_REG_SHIFT) ///< The offset to specify the register to load into the second argument.
#define IOSQE_LOAD2 \
    (IOSQE_LOAD1 + IOSQE_REG_SHIFT) ///< The offset to specify the register to load into the third argument.
#define IOSQE_LOAD3 \
    (IOSQE_LOAD2 + IOSQE_REG_SHIFT) ///< The offset to specify the register to load into the fourth argument.
#define IOSQE_LOAD4 \
    (IOSQE_LOAD3 + IOSQE_REG_SHIFT) ///< The offset to specify the register to load into the fifth argument.
#define IOSQE_SAVE (IOSQE_LOAD4 + IOSQE_REG_SHIFT) ///< The offset to specify the register to save the result into.

#define _IOSQE_FLAGS (IOSQE_SAVE + IOSQE_REG_SHIFT) ///< The bitshift for where bit flags start in a `iosqe_flags_t`.

/**
 * Only process the next SQE when this one completes successfully) only applies within one `enter()` call.
 */
#define IOSQE_LINK (1 << (_IOSQE_FLAGS))
/**
 * Like `IOSQE_LINK` but will process the next SQE even if this one fails.
 */
#define IOSQE_HARDLINK (1 << (_IOSQE_FLAGS + 1))

/**
 * @brief Asynchronous submission queue entry (SQE).
 * @struct iosqe_t
 *
 * @warning It is the responsibility of userspace to ensure that any pointers
 * passed to the kernel remain valid until the operation is complete.
 *
 * @see kernel_io for more information for each possible operation.
 */
typedef struct iosqe
{
    /**
     * Timeout for the operation, `CLOCKS_NEVER` for no timeout, or `0` to fail the operation if it cannot be completed
     * immediately.
     */
    clock_t timeout;
    uintptr_t data;      ///< Private data for the operation, will be returned in the completion entry.
    ioop_t op;           ///< The operation to perform.
    iosqe_flags_t flags; ///< Submission flags.
    union {
        uint64_t arg0;
        fd_t fd;
        uintptr_t target;
    };
    union {
        uint64_t arg1;
        void* buffer;
        ioevents_t events;
        iocancel_t cancel;
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
} iosqe_t;

#ifdef static_assert
static_assert(sizeof(iosqe_t) == 64, "iosqe_t is not 64 bytes");
#endif

/**
 * @brief Macro to create an asynchronous submission queue entry (SQE).
 *
 * @param _op The operation to perform.
 * @param _flags Submission flags.
 * @param _timeout Timeout for the operation, `CLOCKS_NEVER` for no timeout.
 * @param _data Private data for the operation.
 */
#define IOSQE_CREATE(_op, _flags, _timeout, _data) \
    (iosqe_t) \
    { \
        .op = (_op), .flags = (_flags), .timeout = (_timeout), .data = (_data), \
    }

/**
 * @brief I/O Completion Queue Entry (CQE).
 * @struct iocqe_t
 *
 * @see kernel_io for more information on the possible operations.
 */
typedef struct iocqe
{
    ioop_t op;        ///< The operation that was performed.
    status_t status;  ///< The status of the operation.
    uintptr_t result; ///< The result of the operation.
    uintptr_t data;   ///< Private data from the submission entry.
    uint8_t _reserved[8];
} iocqe_t;

#ifdef static_assert
static_assert(sizeof(iocqe_t) == 32, "iocqe_t is not 32 bytes");
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
typedef struct ioring_ctrl
{
    atomic_uint32_t shead; ///< Submission head index, updated by the kernel.
    atomic_uint32_t ctail; ///< Completion tail index, updated by the kernel.
    uint8_t _padding0[64 - sizeof(atomic_uint32_t) * 2];
    atomic_uint32_t stail; ///< Submission tail index, updated by userspace.
    atomic_uint32_t chead; ///< Completion head index, updated by userspace.
    uint8_t _padding1[64 - sizeof(atomic_uint32_t) * 2];
    atomic_uint64_t regs[IOSQE_REGS_MAX] ALIGNED(64); ///< General purpose registers.
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
    iosqe_t* squeue;     ///< Pointer to the submission queue.
    size_t sentries;     ///< Number of entries in the submission queue.
    size_t smask;        ///< Bitmask for submission queue (sentries - 1).
    iocqe_t* cqueue;     ///< Pointer to the completion queue.
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
static inline iosqe_t* iosqe_get(ioring_t* ring)
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
static inline void iosqe_put(ioring_t* ring)
{
    uint32_t tail = atomic_load_explicit(&ring->ctrl->stail, memory_order_relaxed);
    atomic_store_explicit(&ring->ctrl->stail, tail + 1, memory_order_release);
}

/**
 * @brief Retrieve the next available completion queue entry (CQE) from the ring.
 *
 * @param ring The I/O ring.
 * @return On success, a pointer to the next available CQE. If the ring is empty, `NULL`.
 */
static inline iocqe_t* iocqe_get(ioring_t* ring)
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
static inline void iocqe_put(ioring_t* ring)
{
    uint32_t head = atomic_load_explicit(&ring->ctrl->chead, memory_order_relaxed);
    atomic_store_explicit(&ring->ctrl->chead, head + 1, memory_order_release);
}

/**
 * @brief Prepare a no-op submission queue entry (SQE).
 *
 * @see `IOOP_NOP`
 */
static inline void ioprep_nop(iosqe_t* iosqe, iosqe_flags_t flags, clock_t timeout, uintptr_t data)
{
    *iosqe = IOSQE_CREATE(IOOP_NOP, flags, timeout, data);
}

/**
 * @brief Prepare a read submission queue entry (SQE).
 *
 * @see `IOOP_READ`
 */
static inline void ioprep_read(iosqe_t* iosqe, iosqe_flags_t flags, clock_t timeout, uintptr_t data, fd_t fd,
    void* buffer, size_t count, ssize_t offset)
{
    *iosqe = IOSQE_CREATE(IOOP_READ, flags, timeout, data);
    iosqe->fd = fd;
    iosqe->buffer = buffer;
    iosqe->count = count;
    iosqe->offset = offset;
}

/**
 * @brief Prepare a write submission queue entry (SQE).
 *
 * @see `IOOP_WRITE`
 */
static inline void ioprep_write(iosqe_t* iosqe, iosqe_flags_t flags, clock_t timeout, uintptr_t data, fd_t fd,
    const void* buffer, size_t count, ssize_t offset)
{
    *iosqe = IOSQE_CREATE(IOOP_WRITE, flags, timeout, data);
    iosqe->fd = fd;
    iosqe->buffer = (void*)buffer;
    iosqe->count = count;
    iosqe->offset = offset;
}

/**
 * @brief Prepare a poll submission queue entry (SQE).
 *
 * @see `IOOP_POLL`
 */
static inline void ioprep_poll(iosqe_t* iosqe, iosqe_flags_t flags, clock_t timeout, uintptr_t data, fd_t fd,
    ioevents_t events)
{
    *iosqe = IOSQE_CREATE(IOOP_POLL, flags, timeout, data);
    iosqe->fd = fd;
    iosqe->events = events;
}

/**
 * @brief Prepare a cancel submission queue entry (SQE).
 *
 * @see `IOOP_CANCEL`
 */
static inline void ioprep_cancel(iosqe_t* iosqe, iosqe_flags_t flags, clock_t timeout, uintptr_t data, uintptr_t target,
    iocancel_t cancel)
{
    *iosqe = IOSQE_CREATE(IOOP_CANCEL, flags, timeout, data);
    iosqe->target = target;
    iosqe->cancel = cancel;
}

/** @} */

#if defined(__cplusplus)
}
#endif

#endif