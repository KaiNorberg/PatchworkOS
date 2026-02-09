#ifndef _SYS_IORING_H
#define _SYS_IORING_H 1

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/defs.h>
#include <sys/list.h>
#include <sys/proc.h>
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
 * @brief Scriptable submission/completion interface.
 * @defgroup libstd_sys_io I/O Ring ABI
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

typedef uint32_t ioop_t; ///< I/O operation code type.
/**
 * @brief No-op operation.
 *
 * @param Unused
 * @param Unused
 * @param Unused
 * @param Unused
 * @param Unused
 * @result Always `0`.
 */
#define IOOP_NOP 0
/**
 * @brief Cancel operation.
 *
 * @param target The user data of the operation(s) to cancel.
 * @param flags Cancellation flags.
 * @param Unused
 * @param Unused
 * @param Unused
 * @result The number of operations cancelled.
 */
#define IOOP_CANCEL 1
/**
 * @brief Read operation.
 *
 * @param fd The file descriptor to read from.
 * @param buffer The buffer to read into.
 * @param count The number of bytes to read.
 * @param offset The offset to read from, or `IOOFF_CUR`.
 * @param Unused
 * @result The number of bytes read.
 */
#define IOOP_READ 2
/**
 * @brief Write operation.
 *
 * @param fd The file descriptor to write to.
 * @param buffer The buffer to write from.
 * @param count The number of bytes to write.
 * @param offset The offset to write to, or `IOOFF_CUR`.
 * @param Unused
 * @result The number of bytes written.
 */
#define IOOP_WRITE 3
/**
 * @brief Poll operation.
 *
 * @param fd The file descriptor to poll.
 * @param events The events to wait for.
 * @param Unused
 * @param Unused
 * @param Unused
 * @result The events that occurred stored as a `ioevents_t` value.
 */
#define IOOP_POLL 4
/**
 * @brief Seek operation.
 *
 * @param fd The file descriptor to seek.
 * @param Unused
 * @param origin The origin of the seek operation (e.g., `IOSEEK_SET`, `IOSEEK_CUR`, `IOSEEK_END`).
 * @param offset The offset to seek to.
 * @param Unused
 * @result The new file position.
 */
#define IOOP_SEEK 5
/**
 * @brief Memory map operation.
 *
 * @param fd The file descriptor to map.
 * @param address The virtual address to map the file into, or `NULL` for any address.
 * @param count The number of bytes to map.
 * @param offset The offset within the file to start mapping from.
 * @param mem Memory mapping flags.
 * @result The virtual address where the file was mapped.
 */
#define IOOP_MMAP 6
/**
 * @brief Control operation.
 *
 * @param fd The file descriptor to perform the command on.
 * @param command The command to perform.
 * @param args The arguments for the command.
 * @param Unused
 * @param Unused
 * @result The result of the command.
 */
#define IOOP_CONTROL 7
#define IOOP_MAX 8 ///< The maximum number of operation.

#define IOOFF_CUR (((ssize_t) - 1)) ///< Use the current file offset.

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

/**
 * @brief I/O command identifier type.
 *
 * An I/O command is identified by an 8-character zero-padded string packed into a 64-bit integer.
 *
 * This allows commands to be sent directly to files while also allowing control files to parse string input and convert
 * it to their `iocmd_t` representation.
 *
 */
typedef uint64_t iocmd_t;
/**
 * @brief Creates a command value from its text representation.
 *
 * The text will be padded with zeros.
 *
 * @param ... The text representation of the command, up to 8 characters.
 */
#define IOCMD(...) \
    _IOCMD_ANY(__VA_ARGS__, _IOCMD_8, _IOCMD_7, _IOCMD_6, _IOCMD_5, _IOCMD_4, _IOCMD_3, _IOCMD_2, _IOCMD_1)(__VA_ARGS__)

typedef uint64_t iomem_t;    ///< I/O memory map flags.
#define IOMAP_NONE (0)       ///< No flags.
#define IOMAP_READ (1 << 0)  ///< Map for reading.
#define IOMAP_WRITE (1 << 1) ///< Map for writing.
#define IOMAP_EXEC (1 << 2)  ///< Map for execution.

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
        iocmd_t command;
        void* address;
    };
    union {
        uint64_t arg2;
        size_t count;
        const char* args;
        iowhence_t origin;
    };
    union {
        uint64_t arg3;
        ssize_t offset;
    };
    union {
        uint64_t arg4;
        iomem_t mem;
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
 * @brief Prepare a seek submission queue entry (SQE).
 *
 * @see `IOOP_SEEK`
 */
static inline void ioprep_seek(iosqe_t* iosqe, iosqe_flags_t flags, clock_t timeout, uintptr_t data, fd_t fd,
    iowhence_t origin, ssize_t offset)
{
    *iosqe = IOSQE_CREATE(IOOP_SEEK, flags, timeout, data);
    iosqe->fd = fd;
    iosqe->origin = origin;
    iosqe->offset = offset;
}

/**
 * @brief Prepare a memory map submission queue entry (SQE).
 *
 * @see `IOOP_MMAP`
 */
static inline void ioprep_mmap(iosqe_t* iosqe, iosqe_flags_t flags, clock_t timeout, uintptr_t data, fd_t fd,
    void* address, size_t count, ssize_t offset, iomem_t mem)
{
    *iosqe = IOSQE_CREATE(IOOP_MMAP, flags, timeout, data);
    iosqe->fd = fd;
    iosqe->address = address;
    iosqe->count = count;
    iosqe->offset = offset;
    iosqe->mem = mem;
}

/**
 * @brief Prepare a control submission queue entry (SQE).
 *
 * @see `IOOP_CONTROL`
 */
static inline void ioprep_control(iosqe_t* iosqe, iosqe_flags_t flags, clock_t timeout, uintptr_t data, fd_t fd,
    iocmd_t command, const char* args)
{
    *iosqe = IOSQE_CREATE(IOOP_CONTROL, flags, timeout, data);
    iosqe->fd = fd;
    iosqe->command = command;
    iosqe->args = args;
}

/**
 * @brief Synchronous wrapper for I/O ring operations.
 *
 * Will use a standard library defined per-process ring to perform the operation synchronously.
 *
 * @param sqe The submission queue entry to perform.
 * @param cqe Output pointer for the completion queue entry.
 */
void iosync(iosqe_t* sqe, iocqe_t* cqe);

/**
 * @brief Synchronous wrapper for multiple I/O ring operations.
 *
 * Will use a standard library defined per-process ring to perform multiple operations synchronously.
 *
 * @param sqes Array of submission queue entries.
 * @param cqes Array of completion queue entries.
 * @param count Number of entries.
 * @param wait Minimum number of completions to wait for.
 * @param completed Output pointer for the number of completed entries.
 */
void iosync_many(iosqe_t* sqes, iocqe_t* cqes, size_t count, size_t wait, size_t* completed);

/**
 * @brief Synchronous wrapper for a read operation.
 *
 * @param fd The file descriptor to read from.
 * @param buffer The buffer to read into.
 * @param count The number of bytes to read.
 * @param offset The offset to read from, or `IOOFF_CUR`.
 * @param bytesRead Output pointer for the number of bytes read, can be `NULL`.
 * @return An appropriate status value.
 */
static inline status_t ioread(fd_t fd, void* buffer, size_t count, ssize_t offset, size_t* bytesRead)
{
    iosqe_t sqe;
    iocqe_t cqe;
    ioprep_read(&sqe, IOSQE_NORMAL, CLOCKS_NEVER, 0, fd, buffer, count, offset);
    iosync(&sqe, &cqe);
    if (bytesRead != NULL)
    {
        *bytesRead = cqe.result;
    }
    return cqe.status;
}

/**
 * @brief Synchronous wrapper for a write operation.
 *
 * @param fd The file descriptor to write to.
 * @param buffer The buffer to write from.
 * @param count The number of bytes to write.
 * @param offset The offset to write to, or `IOOFF_CUR`.
 * @param bytesWritten Output pointer for the number of bytes written, can be `NULL`.
 * @return An appropriate status value.
 */
static inline status_t iowrite(fd_t fd, const void* buffer, size_t count, ssize_t offset, size_t* bytesWritten)
{
    iosqe_t sqe;
    iocqe_t cqe;
    ioprep_write(&sqe, IOSQE_NORMAL, CLOCKS_NEVER, 0, fd, buffer, count, offset);
    iosync(&sqe, &cqe);
    if (bytesWritten != NULL)
    {
        *bytesWritten = cqe.result;
    }
    return cqe.status;
}

/**
 * @brief Synchronous wrapper for a poll operation.
 *
 * @param fd The file descriptor to poll.
 * @param timeout Timeout for the operation, `CLOCKS_NEVER` for no timeout.
 * @param events The events to wait for.
 * @param revents Output pointer for the events that occurred, can be `NULL`.
 * @return An appropriate status value.
 */
static inline status_t iopoll(fd_t fd, clock_t timeout, ioevents_t events, ioevents_t* revents)
{
    iosqe_t sqe;
    iocqe_t cqe;
    ioprep_poll(&sqe, IOSQE_NORMAL, timeout, 0, fd, events);
    iosync(&sqe, &cqe);
    if (revents != NULL)
    {
        *revents = (ioevents_t)cqe.result;
    }
    return cqe.status;
}
/**
 * @brief Poll file descriptor structure.
 * @struct iopoll_t
 */
typedef struct iopoll
{
    fd_t fd;            ///< The file descriptor to poll.
    ioevents_t events;  ///< The events to wait for.
    ioevents_t revents; ///< The events that occurred.
} iopoll_t;

/**
 * @brief Synchronous wrapper for polling multiple files.
 *
 * @param fds Array of pollfd structures.
 * @param nfds Number of file descriptors.
 * @param timeout Timeout in clock ticks.
 * @param count Output pointer for the number of events.
 * @return An appropriate status value.
 */
status_t iopoll_many(iopoll_t* fds, size_t nfds, clock_t timeout, size_t* count);

/**
 * @brief Synchronous wrapper for a seek operation.
 *
 * @param fd The file descriptor to seek.
 * @param origin The origin of the seek operation.
 * @param offset The offset to seek to.
 * @param pos Output pointer for the new file position, can be `NULL`.
 * @return An appropriate status value.
 */
static inline status_t ioseek(fd_t fd, iowhence_t origin, ssize_t offset, size_t* pos)
{
    iosqe_t sqe;
    iocqe_t cqe;
    ioprep_seek(&sqe, IOSQE_NORMAL, CLOCKS_NEVER, 0, fd, origin, offset);
    iosync(&sqe, &cqe);
    if (pos != NULL)
    {
        *pos = (size_t)cqe.result;
    }
    return cqe.status;
}

/**
 * @brief Convert a size in bytes to pages.
 *
 * @param amount The amount of bytes.
 * @return The amount of pages.
 */
#define BYTES_TO_PAGES(amount) (((amount) + PAGE_SIZE - 1) / PAGE_SIZE)

/**
 * @brief Size of an object in pages.
 *
 * @param object The object to calculate the page size of.
 * @return The amount of pages.
 */
#define PAGE_SIZE_OF(object) BYTES_TO_PAGES(sizeof(object))

/**
 * @brief Synchronous wrapper for a memory map operation.
 *
 * @param fd The file descriptor to map.
 * @param address Input/Output pointer for the virtual address, if *address` is `NULL` the kernel will choose an
 * address.
 * @param count The number of bytes to map.
 * @param offset The offset within the file to start mapping from.
 * @param mmap Memory mapping flags.
 * @return An appropriate status value.
 */
static inline status_t iomap(fd_t fd, void** address, size_t count, ssize_t offset, iomem_t mmap)
{
    if ((void*)address == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    iosqe_t sqe;
    iocqe_t cqe;
    ioprep_mmap(&sqe, IOSQE_NORMAL, CLOCKS_NEVER, 0, fd, *address, count, offset, mmap);
    iosync(&sqe, &cqe);
    *address = (void*)cqe.result;
    return cqe.status;
}

/**
 * @brief System call to unmap mapped memory.
 *
 * The `iounmap()` function unmaps memory from the currently running processes address space.
 *
 * @param address The starting virtual address of the memory area to be unmapped.
 * @param length The length of the memory area to be unmapped.
 * @return An appropriate status value.
 */
static inline status_t iounmap(void* address, size_t length)
{
    return syscall2(SYS_IO_UNMAP, NULL, (uintptr_t)address, length);
}

/**
 * @brief System call to change the protection flags of memory.
 *
 * @param address  The starting virtual address of the memory area to be modified.
 * @param length The length of the memory area to be modifed.
 * @param map The new protection flags of the memory area, if equal to `IOMAP_NONE` the memory area will be
 * unmapped.
 * @return An appropriate status value.
 */
static inline status_t ioprotect(void* address, size_t length, iomem_t map)
{
    return syscall3(SYS_IO_PROTECT, NULL, (uintptr_t)address, length, map);
}

/** @} */

#define _IO_B(x, n) (((uint64_t)(x)) << (8 * n))

#define _IOCMD_1(a) (_IO_B(a, 0))
#define _IOCMD_2(a, b) (_IO_B(a, 0) | _IO_B(b, 1))
#define _IOCMD_3(a, b, c) (_IO_B(a, 0) | _IO_B(b, 1) | _IO_B(c, 2))
#define _IOCMD_4(a, b, c, d) (_IO_B(a, 0) | _IO_B(b, 1) | _IO_B(c, 2) | _IO_B(d, 3))
#define _IOCMD_5(a, b, c, d, e) (_IO_B(a, 0) | _IO_B(b, 1) | _IO_B(c, 2) | _IO_B(d, 3) | _IO_B(e, 4))
#define _IOCMD_6(a, b, c, d, e, f) (_IO_B(a, 0) | _IO_B(b, 1) | _IO_B(c, 2) | _IO_B(d, 3) | _IO_B(e, 4) | _IO_B(f, 5))
#define _IOCMD_7(a, b, c, d, e, f, g) \
    (_IO_B(a, 0) | _IO_B(b, 1) | _IO_B(c, 2) | _IO_B(d, 3) | _IO_B(e, 4) | _IO_B(f, 5) | _IO_B(g, 6))
#define _IOCMD_8(a, b, c, d, e, f, g, h) \
    (_IO_B(a, 0) | _IO_B(b, 1) | _IO_B(c, 2) | _IO_B(d, 3) | _IO_B(e, 4) | _IO_B(f, 5) | _IO_B(g, 6) | _IO_B(h, 7))

#define _IOCMD_ANY(_1, _2, _3, _4, _5, _6, _7, _8, NAME, ...) NAME

#if defined(__cplusplus)
}
#endif

#endif