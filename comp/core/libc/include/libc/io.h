#ifndef _SYS_IO_H
#define _SYS_IO_H 1

#include <libc/defs.h>
#include <libc/fs.h>
#include <libc/list.h>
#include <libc/proc.h>
#include <libc/status.h>
#include <libc/syscall.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <threads.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libc/MAX_NAME.h"
#include "_libc/MAX_PATH.h"
#include "_libc/clock_t.h"
#include "_libc/errno_t.h"
#include "_libc/ssize_t.h"

char* _thread_get_iofmt(void);

/**
 * @brief Userspace I/O interface.
 * @defgroup libc_io I/O Ring ABI
 * @ingroup libc
 *
 * @todo Write I/O Ring user-side interface documentation.
 *
 * @see kernel_io_ioring for more information about I/O rings.
 *
 * @{
 */

typedef uint32_t ioop_t; ///< I/O operation code type.

/**
 * @brief Cancel operation.
 * @param target The user data of the operation(s) to cancel.
 * @param cancel Cancellation flags.
 * @param Unused
 * @param Unused
 * @param Unused
 * @result The number of operations cancelled.
 */
#define IOOP_CANCEL 0

/**
 * @brief Read operation.
 *
 * @note Reading for a directory will return a stream of null deliminated strings, with each string representing the
 * name of a file within the directory.
 *
 * @param fd The file descriptor to read from.
 * @param vector An array of `iovec_t` structures to read into.
 * @param count The number of `iovec_t` structures.
 * @param offset The offset to read from, or `IOCUR`.
 * @param Unused
 * @result The number of bytes read.
 */
#define IOOP_READ 1

/**
 * @brief Write operation.
 * @param fd The file descriptor to write to.
 * @param vector An array of `iovec_t` structures to write from.
 * @param count The number of `iovec_t` structures.
 * @param offset The offset to write to, or `IOCUR`.
 * @param Unused
 * @result The number of bytes written.
 */
#define IOOP_WRITE 2

/**
 * @brief Poll operation.
 * @param fd The file descriptor to poll.
 * @param events The events to wait for.
 * @param Unused
 * @param Unused
 * @param Unused
 * @result The events that occurred stored as a `iopoll_t` value.
 */
#define IOOP_POLL 3

/**
 * @brief Seek operation.
 * @param fd The file descriptor to seek.
 * @param Unused
 * @param origin The origin of the seek operation (e.g., `IOSEEK_START`, `IOSEEK_CUR`, `IOSEEK_END`).
 * @param offset The offset to seek to.
 * @param Unused
 * @result The new file position.
 */
#define IOOP_SEEK 4

/**
 * @brief Memory map operation.
 * @param fd The file descriptor to map.
 * @param address The virtual address to map the file into, or `NULL` for any address.
 * @param count The number of bytes to map.
 * @param offset The offset within the file to start mapping from.
 * @param mem Memory mapping flags.
 * @result The virtual address where the file was mapped.
 */
#define IOOP_MAP 5

/**
 * @brief Walk operation.
 *
 * Traverse the filesystem and open a file descriptor to the reached location.
 *
 * @param cwd The file descriptor to open the file relative to, or `FDCWD` to open from the current working directory.
 * @param root The file descriptor to use as the root for the walk, or `FDROOT` to use the standard root directory.
 * @param path The path to the file to open, also contains flags @see kernel_fs_path.
 * @param count The length of the path.
 * @param Unused
 * @result The opened file descriptor.
 */
#define IOOP_WALK 6

/**
 * @brief Drop operation.
 * @param fd The file descriptor to drop.
 * @param Unused
 * @param Unused
 * @param Unused
 * @param Unused
 * @result Always `0`.
 */
#define IOOP_DROP 7

/**
 * @brief Remove operation.
 *
 * @note Will remove a file from the filesystem hierarchy, the files underlying resources will only be released once all
 * file descriptors referencing it are closed.
 *
 * @param fd The file descriptor to a file to remove.
 * @param Unused
 * @param Unused
 * @param Unused
 * @param Unused
 * @result Always `0`.
 */
#define IOOP_REMOVE 8

/**
 * @brief File attribute operation.
 * @param fd The file descriptor.
 * @param attr The attribute to get or set (e.g., FILE_GET_SIZE, FILE_SET_SIZE).
 * @param value The value to set (ignored for getters).
 * @param Unused
 * @param Unused
 * @result The requested value or `0` if setting a value.
 */
#define IOOP_ATTR 9

/**
 * @brief File query operation.
 * @param fd The file descriptor.
 * @param info Pointer to the `file_info_t` structure to fill.
 * @param Unused
 * @param Unused
 * @param Unused
 * @result Always `0`.
 */
#define IOOP_QUERY 10

/**
 * @brief Flush operation.
 * @param fd The file descriptor to flush.
 * @param Unused
 * @param Unused
 * @param Unused
 * @param Unused
 * @result Always `0`.
 */
#define IOOP_FLUSH 11

#define IOOP_MAX 12 ///< The maximum number of operations.

#define IOCUR (((ssize_t) - 1)) ///< Use the current file offset.

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

typedef uint64_t iomap_t;    ///< I/O memory map flags.
#define IOMAP_NONE (0)       ///< No flags.
#define IOMAP_READ (1 << 0)  ///< Map for reading.
#define IOMAP_WRITE (1 << 1) ///< Map for writing.
#define IOMAP_EXEC (1 << 2)  ///< Map for execution.

typedef uint64_t iocancel_t;  ///< Cancel operation flags.
#define IOCANCEL_ALL (1 << 0) ///< Cancel all matching requests.
#define IOCANCEL_ANY (1 << 1) ///< Match any user data.

typedef uint64_t ioseek_t;           ///< Seek origin type.
#define IOSEEK_START ((ioseek_t)0)   ///< Seek from the beginning of the file.
#define IOSEEK_CURRENT ((ioseek_t)1) ///< Seek from the current position.
#define IOSEEK_END ((ioseek_t)2)     ///< Seek from the end of the file.

typedef uint64_t ioevents_t;   ///< Events type.
#define IOEVENT_READ (1 << 0)  ///< File descriptor is ready to be read from.
#define IOEVENT_WRITE (1 << 1) ///< File descriptor is ready to be written to.
#define IOEVENT_ERROR (1 << 2) ///< File descriptor caused an error.
#define IOEVENT_HUP (1 << 3)   ///< File descriptor is closed.
#define IOEVENT_NVAL (1 << 4)  ///< Invalid file descriptor.

/**
 * @brief I/O vector structure.
 * @struct iovec_t
 */
typedef struct iovec
{
    void* base;    ///< Pointer to the buffer.
    size_t length; ///< Length of the buffer.
} iovec_t;

/**
 * @brief Helper macro for passing a simple buffer to `ioread()` or `iowrite()`.
 *
 * This macro expands to a pointer to a temporary `iovec_t` and a count of 1.
 *
 * @param ptr Pointer to the buffer.
 * @param len Length of the buffer.
 */
#define IOBUF(ptr, len) &((iovec_t){.base = (void*)(ptr), .length = (size_t)(len)}), 1

/**
 * @brief Helper macro for passing standard path arguments.
 *
 * Expands to FDCWD, FDROOT, and the provided path.
 *
 * @param path The path string.
 */
#define IOPATH(path) FDCWD, FDROOT, (path)

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
     * Timeout for the operation, `CLOCKS_NEVER` for no timeout, or `CLOCKS_NOW` to fail the operation if it cannot be
     * completed immediately.
     */
    clock_t timeout;
    uintptr_t data;      ///< Private data for the operation, will be returned in the completion entry.
    ioop_t op;           ///< The operation to perform.
    iosqe_flags_t flags; ///< Submission flags.
    union {
        uint64_t arg0;
        fd_t fd;
        fd_t cwd;
        uintptr_t target;
    };
    union {
        uint64_t arg1;
        const iovec_t* vector;
        ioevents_t events;
        iocancel_t cancel;
        iocmd_t command;
        void* address;
        file_attr_t attr;
        file_info_t* info;
        fd_t root;
    };
    union {
        uint64_t arg2;
        size_t count;
        ioseek_t origin;
        uint64_t value;
        const char* path;
    };
    union {
        uint64_t arg3;
        ssize_t offset;
        size_t pathLen;
    };
    union {
        uint64_t arg4;
        iomap_t map;
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
 * @brief Poll file descriptor structure.
 * @struct iopoll_t
 */
typedef struct iopoll
{
    fd_t fd;            ///< The file descriptor to poll.
    ioevents_t events;  ///< The events to wait for.
    ioevents_t revents; ///< The events that occurred.
} iopoll_t;

#ifndef _KERNEL_

/**
 * @brief Maximum buffer size for the `IOFMT()` macro.
 */
#define IOFMT_MAX 256

/**
 * @brief Maximum amount of per-thread buffers for the `IOFMT()` macro.
 */
#define IOFMT_BUFFERS_MAX 8

/**
 * @brief Allocates a formatted string using a set of thread-local ring buffers.
 *
 * @warning Will terminate the program if the size of the formatted string is too large or if an encoding error occurs.
 *
 * @warning Up to `IOFMT_BUFFERS_MAX` formats can be safely used before the first one is overwritten.
 */
#define IOFMT(format, ...) \
    ({ \
        char* _buffer = _thread_get_iofmt(); \
        int _len = snprintf(_buffer, IOFMT_MAX, format, __VA_ARGS__); \
        if (_len < 0 || _len >= IOFMT_MAX) \
        { \
            abort(); \
        } \
        _buffer; \
    })

#ifndef _IORING_GET
/**
 * @brief Internal helper to get the I/O ring for the current thread.
 *
 * @note This function shouldent be used directly within the I/O header, instead the `_IORING_GET()` macro should be
 * used. This is primarily so the dynamic linker can also use the I/O header by defining it themselves.
 *
 * @return The I/O ring for the current thread.
 */
PURE ioring_t* _ioring_get(void);
#define _IORING_GET() _ioring_get()
#endif

#ifndef _IORING_STRLEN
#define _IORING_STRLEN(s) strlen(s)
#endif

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
 * @brief Set the active I/O ring for the current thread.
 *
 * @param ring The ring to set as active, or `NULL` to use the default thread-specific ring.
 * @return The previously active ring, or `NULL` if it was the default ring.
 */
ioring_t* ioring_set(ioring_t* ring);

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
 * @brief Retrieve the number of pending submission queue entries (SQEs) in the ring.
 *
 * @param ring The I/O ring.
 * @return The number of pending SQEs.
 */
static inline size_t iosqe_count(ioring_t* ring)
{
    uint32_t tail = atomic_load_explicit(&ring->ctrl->stail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&ring->ctrl->shead, memory_order_acquire);
    return tail - head;
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
    const iovec_t* vector, size_t count, ssize_t offset)
{
    *iosqe = IOSQE_CREATE(IOOP_READ, flags, timeout, data);
    iosqe->fd = fd;
    iosqe->vector = vector;
    iosqe->count = count;
    iosqe->offset = offset;
}

/**
 * @brief Prepare a write submission queue entry (SQE).
 *
 * @see `IOOP_WRITE`
 */
static inline void ioprep_write(iosqe_t* iosqe, iosqe_flags_t flags, clock_t timeout, uintptr_t data, fd_t fd,
    const iovec_t* vector, size_t count, ssize_t offset)
{
    *iosqe = IOSQE_CREATE(IOOP_WRITE, flags, timeout, data);
    iosqe->fd = fd;
    iosqe->vector = vector;
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
    ioseek_t origin, ssize_t offset)
{
    *iosqe = IOSQE_CREATE(IOOP_SEEK, flags, timeout, data);
    iosqe->fd = fd;
    iosqe->origin = origin;
    iosqe->offset = offset;
}

/**
 * @brief Prepare a memory map submission queue entry (SQE).
 *
 * @see `IOOP_MAP`
 */
static inline void ioprep_map(iosqe_t* iosqe, iosqe_flags_t flags, clock_t timeout, uintptr_t data, fd_t fd,
    void* address, size_t count, ssize_t offset, iomap_t map)
{
    *iosqe = IOSQE_CREATE(IOOP_MAP, flags, timeout, data);
    iosqe->fd = fd;
    iosqe->address = address;
    iosqe->count = count;
    iosqe->offset = offset;
    iosqe->map = map;
}

/**
 * @brief Prepare an walk submission queue entry (SQE).
 *
 * @see `IOOP_WALK`
 */
static inline void ioprep_walk(iosqe_t* iosqe, iosqe_flags_t flags, clock_t timeout, uintptr_t data, fd_t cwd,
    fd_t root, const char* path, size_t count)
{
    *iosqe = IOSQE_CREATE(IOOP_WALK, flags, timeout, data);
    iosqe->cwd = cwd;
    iosqe->root = root;
    iosqe->path = path;
    iosqe->pathLen = count;
}

/**
 * @brief Prepare a drop submission queue entry (SQE).
 *
 * @see `IOOP_DROP`
 */
static inline void ioprep_drop(iosqe_t* iosqe, iosqe_flags_t flags, clock_t timeout, uintptr_t data, fd_t fd)
{
    *iosqe = IOSQE_CREATE(IOOP_DROP, flags, timeout, data);
    iosqe->fd = fd;
}

/**
 * @brief Prepare a remove submission queue entry (SQE).
 *
 * @see `IOOP_REMOVE`
 */
static inline void ioprep_remove(iosqe_t* iosqe, iosqe_flags_t flags, clock_t timeout, uintptr_t data, fd_t fd)
{
    *iosqe = IOSQE_CREATE(IOOP_REMOVE, flags, timeout, data);
    iosqe->fd = fd;
}

/**
 * @brief Prepare an attribute submission queue entry (SQE).
 *
 * @see `IOOP_ATTR`
 */
static inline void ioprep_attr(iosqe_t* iosqe, iosqe_flags_t flags, clock_t timeout, uintptr_t data, fd_t fd,
    file_attr_t attr, uint64_t value)
{
    *iosqe = IOSQE_CREATE(IOOP_ATTR, flags, timeout, data);
    iosqe->fd = fd;
    iosqe->attr = attr;
    iosqe->value = value;
}

/**
 * @brief Prepare a query submission queue entry (SQE).
 *
 * @see `IOOP_QUERY`
 */
static inline void ioprep_query(iosqe_t* iosqe, iosqe_flags_t flags, clock_t timeout, uintptr_t data, fd_t fd,
    file_info_t* info)
{
    *iosqe = IOSQE_CREATE(IOOP_QUERY, flags, timeout, data);
    iosqe->fd = fd;
    iosqe->info = info;
}

/**
 * @brief Prepare a flush submission queue entry (SQE).
 *
 * @see `IOOP_FLUSH`
 */
static inline void ioprep_flush(iosqe_t* iosqe, iosqe_flags_t flags, clock_t timeout, uintptr_t data, fd_t fd)
{
    *iosqe = IOSQE_CREATE(IOOP_FLUSH, flags, timeout, data);
    iosqe->fd = fd;
}

/**
 * @brief Internal helper to get a submission queue entry, entering the ring if full.
 *
 * @return A pointer to the next available SQE.
 */
static inline iosqe_t* _iosqe_get(ioring_t* ring)
{
    iosqe_t* sqe;
    while ((sqe = iosqe_get(ring)) == NULL)
    {
        ioring_enter(ring, iosqe_count(ring), 0, NULL);
    }
    return sqe;
}

/**
 * @brief Internal helper to get the last submission queue entry.
 *
 * @return A pointer to the last SQE.
 */
static inline iosqe_t* _iosqe_last(ioring_t* ring)
{
    uint32_t tail = atomic_load_explicit(&ring->ctrl->stail, memory_order_relaxed);
    return &ring->squeue[(tail - 1) & ring->smask];
}

/**
 * @brief I/O Register type for configuration options.
 * @enum ioreg_t
 */
typedef enum
{
    IOREG_NONE = IOSQE_REG_NONE,
    IOREG0 = IOSQE_REG0,
    IOREG1 = IOSQE_REG1,
    IOREG2 = IOSQE_REG2,
    IOREG3 = IOSQE_REG3,
    IOREG4 = IOSQE_REG4,
    IOREG5 = IOSQE_REG5,
    IOREG6 = IOSQE_REG6,
} ioreg_t;

/**
 * @brief I/O Variable type for configuration options.
 */
typedef struct iovar* iovar_t;

#define IONOLINK 0 ///< No link.
#define IOSOFT 1   ///< Soft link (only process next if this one succeeds).
#define IOHARD 2   ///< Hard link (process next even if this one fails).

/**
 * @brief Internal macro to extract the register index from an I/O variable or default to `IOREG_NONE`.
 *
 * @param x The value or `iovar_t` to extract from.
 * @return The extracted register index.
 */
#define _IOVAR_REG(x) _Generic((x), iovar_t: (uint32_t)(uintptr_t)(x), default: IOREG_NONE)

/**
 * @brief Internal macro to return a placeholder value from an I/O variable or default to the provided value.
 *
 * @param x The value or `iovar_t` to extract from.
 * @return The extracted value.
 */
#define _IOVAR_VAL(x) _Generic((x), iovar_t: 0ULL, default: (uint64_t)(size_t)(x))

/**
 * @brief Internal macro to extract the register index from a pointer to an I/O variable or default to `IOREG_NONE`.
 *
 * @param x The pointer to `iovar_t` to extract from.
 * @return The extracted register index.
 */
#define _IOSAVE_REG(x) \
    _Generic((x), \
        iovar_t*: ((uintptr_t)(x)) ? (uint32_t)(uintptr_t)(*(volatile iovar_t*)(uintptr_t)(x)) : (uint32_t)IOREG_NONE, \
        default: IOREG_NONE)

/**
 * @brief Internal macro to convert the shortform I/O link type to their SQE flags expansion.
 *
 * @param _link The link type.
 * @return The corresponding SQE flags.
 */
#define _IOLINK_FLAGS(_link) ((_link) == IOSOFT ? IOSQE_LINK : ((_link) == IOHARD ? IOSQE_HARDLINK : 0))

/**
 * @brief Internal macro to generate the SQE flags for an operation.
 *
 * This macro combines the link flags, the save register, and the load registers for each argument.
 *
 * @param _link The link type.
 * @param _save The register to save the result into.
 * @param _a0 The first argument.
 * @param _a1 The second argument.
 * @param _a2 The third argument.
 * @param _a3 The fourth argument.
 * @param _a4 The fifth argument.
 * @return The generated SQE flags.
 */
#define _IOOPTS_FLAGS(_link, _save, _a0, _a1, _a2, _a3, _a4) \
    (_IOLINK_FLAGS(_link) | ((_IOSAVE_REG(_save) & IOSQE_REG_MASK) << IOSQE_SAVE) | \
        ((_IOVAR_REG(_a0) & IOSQE_REG_MASK) << IOSQE_LOAD0) | ((_IOVAR_REG(_a1) & IOSQE_REG_MASK) << IOSQE_LOAD1) | \
        ((_IOVAR_REG(_a2) & IOSQE_REG_MASK) << IOSQE_LOAD2) | ((_IOVAR_REG(_a3) & IOSQE_REG_MASK) << IOSQE_LOAD3) | \
        ((_IOVAR_REG(_a4) & IOSQE_REG_MASK) << IOSQE_LOAD4))

/**
 * @brief Load a value from an I/O ring register.
 *
 * @param _reg The register to read from.
 * @return The value of the register, or 0 if `IOREG_NONE`.
 */
#define IOREG_LOAD(_reg) \
    ({ \
        uint64_t value = 0; \
        ioreg_t _r = _Generic((_reg), iovar_t: (ioreg_t)(uintptr_t)(_reg), default: (ioreg_t)(size_t)(_reg)); \
        if (_r != IOREG_NONE) \
        { \
            ioring_t* ring = _IORING_GET(); \
            value = atomic_load_explicit(&ring->ctrl->regs[_r - 1], memory_order_relaxed); \
        } \
        value; \
    })

/**
 * @brief Store a value to an I/O ring register.
 *
 * @param _reg The register to write to.
 * @param _value The value to write.
 * @return The register that was written to.
 */
#define IOREG_STORE(_reg, _value) \
    ({ \
        ioreg_t _r = _Generic((_reg), iovar_t: (ioreg_t)(uintptr_t)(_reg), default: (ioreg_t)(size_t)(_reg)); \
        if (_r != IOREG_NONE) \
        { \
            ioring_t* ring = _IORING_GET(); \
            atomic_store_explicit(&ring->ctrl->regs[_r - 1], (uint64_t)(_value), memory_order_relaxed); \
        } \
        _r; \
    })

/**
 * @brief Initialize a register and return an I/O variable representing it.
 *
 * @param _reg The register.
 * @param _value The value to store.
 * @return An `iovar_t` mapped to the specified register.
 */
#define IOREG(_reg, _value) \
    ({ \
        IOREG_STORE(_reg, _value); \
        (iovar_t)(uintptr_t)(_reg); \
    })

/**
 * @brief Queue a cancel operation.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @param _target The user data of the operation(s) to cancel.
 * @param _cancel Cancellation flags.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOCANCELQ(_target, _cancel, _link, _save, _data) IOCANCELQT(_target, _cancel, CLOCKS_NEVER, _link, _save, _data)

/**
 * @brief Queue a cancel operation with a timeout.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @param _target The user data of the operation(s) to cancel.
 * @param _cancel Cancellation flags.
 * @param _timeout The timeout for the operation.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOCANCELQT(_target, _cancel, _timeout, _link, _save, _data) \
    ({ \
        ioring_t* _ring = _IORING_GET(); \
        iosqe_t* _sqe = _iosqe_get(_ring); \
        ioprep_cancel(_sqe, _IOOPTS_FLAGS(_link, _save, _target, _cancel, 0, 0, 0), (_timeout), (uintptr_t)(_data), \
            (uintptr_t)_IOVAR_VAL(_target), (iocancel_t)_IOVAR_VAL(_cancel)); \
        iosqe_put(_ring); \
    })

/**
 * @brief Queue a read operation.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @note The reason for the redirection to the `_*_IMPL` macro is to ensure that `IOBUF()` expands correctly.
 *
 * @param _fd The file descriptor to read from.
 * @param _vector An array of `iovec_t` structures to read into.
 * @param _count The number of `iovec_t` structures.
 * @param _offset The offset to read from, or `IOCUR`.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOREADQ(...) _IOREADQ_IMPL(__VA_ARGS__)
#define _IOREADQ_IMPL(_fd, _vector, _count, _offset, _link, _save, _data) \
    _IOREADQT_IMPL(_fd, _vector, _count, _offset, CLOCKS_NEVER, _link, _save, _data)

/**
 * @brief Queue a read operation with a timeout.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @note The reason for the redirection to the `_*_IMPL` macro is to ensure that `IOBUF()` expands correctly.
 *
 * @param _fd The file descriptor to read from.
 * @param _vector An array of `iovec_t` structures to read into.
 * @param _count The number of `iovec_t` structures.
 * @param _offset The offset to read from, or `IOCUR`.
 * @param _timeout The timeout for the operation.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOREADQT(...) _IOREADQT_IMPL(__VA_ARGS__)
#define _IOREADQT_IMPL(_fd, _vector, _count, _offset, _timeout, _link, _save, _data) \
    ({ \
        ioring_t* _ring = _IORING_GET(); \
        iosqe_t* _sqe = _iosqe_get(_ring); \
        ioprep_read(_sqe, _IOOPTS_FLAGS(_link, _save, _fd, _vector, _count, _offset, 0), (_timeout), \
            (uintptr_t)(_data), (fd_t)_IOVAR_VAL(_fd), (const iovec_t*)_IOVAR_VAL(_vector), \
            (size_t)_IOVAR_VAL(_count), (ssize_t)_IOVAR_VAL(_offset)); \
        iosqe_put(_ring); \
    })

/**
 * @brief Queue a write operation.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @note The reason for the redirection to the `_*_IMPL` macro is to ensure that `IOBUF()` expands correctly.
 *
 * @param _fd The file descriptor to write to.
 * @param _vector An array of `iovec_t` structures to write from.
 * @param _count The number of `iovec_t` structures.
 * @param _offset The offset to write to, or `IOCUR`.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOWRITEQ(...) _IOWRITEQ_IMPL(__VA_ARGS__)
#define _IOWRITEQ_IMPL(_fd, _vector, _count, _offset, _link, _save, _data) \
    _IOWRITEQT_IMPL(_fd, _vector, _count, _offset, CLOCKS_NEVER, _link, _save, _data)

/**
 * @brief Queue a write operation with a timeout.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @note The reason for the redirection to the `_*_IMPL` macro is to ensure that `IOBUF()` expands correctly.
 *
 * @param _fd The file descriptor to write to.
 * @param _vector An array of `iovec_t` structures to write from.
 * @param _count The number of `iovec_t` structures.
 * @param _offset The offset to write to, or `IOCUR`.
 * @param _timeout The timeout for the operation.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOWRITEQT(...) _IOWRITEQT_IMPL(__VA_ARGS__)
#define _IOWRITEQT_IMPL(_fd, _vector, _count, _offset, _timeout, _link, _save, _data) \
    ({ \
        ioring_t* _ring = _IORING_GET(); \
        iosqe_t* _sqe = _iosqe_get(_ring); \
        ioprep_write(_sqe, _IOOPTS_FLAGS(_link, _save, _fd, _vector, _count, _offset, 0), (_timeout), \
            (uintptr_t)(_data), (fd_t)_IOVAR_VAL(_fd), (const iovec_t*)_IOVAR_VAL(_vector), \
            (size_t)_IOVAR_VAL(_count), (ssize_t)_IOVAR_VAL(_offset)); \
        iosqe_put(_ring); \
    })

/**
 * @brief Queue a poll operation.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @param _fd The file descriptor to poll.
 * @param _events The events to wait for.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOPOLLQ(_fd, _events, _link, _save, _data) IOPOLLQT(_fd, _events, CLOCKS_NEVER, _link, _save, _data)

/**
 * @brief Queue a poll operation with a timeout.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @param _fd The file descriptor to poll.
 * @param _events The events to wait for.
 * @param _timeout The timeout for the operation.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOPOLLQT(_fd, _events, _timeout, _link, _save, _data) \
    ({ \
        ioring_t* _ring = _IORING_GET(); \
        iosqe_t* _sqe = _iosqe_get(_ring); \
        ioprep_poll(_sqe, _IOOPTS_FLAGS(_link, _save, _fd, _events, 0, 0, 0), (_timeout), (uintptr_t)(_data), \
            (fd_t)_IOVAR_VAL(_fd), (ioevents_t)_IOVAR_VAL(_events)); \
        iosqe_put(_ring); \
    })

/**
 * @brief Queue a seek operation.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @param _fd The file descriptor to seek.
 * @param _origin The origin of the seek operation.
 * @param _offset The offset to seek to.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOSEEKQ(_fd, _origin, _offset, _link, _save, _data) \
    IOSEEKQT(_fd, _origin, _offset, CLOCKS_NEVER, _link, _save, _data)

/**
 * @brief Queue a seek operation with a timeout.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @param _fd The file descriptor to seek.
 * @param _origin The origin of the seek operation.
 * @param _offset The offset to seek to.
 * @param _timeout The timeout for the operation.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOSEEKQT(_fd, _origin, _offset, _timeout, _link, _save, _data) \
    ({ \
        ioring_t* _ring = _IORING_GET(); \
        iosqe_t* _sqe = _iosqe_get(_ring); \
        ioprep_seek(_sqe, _IOOPTS_FLAGS(_link, _save, _fd, 0, _origin, _offset, 0), (_timeout), (uintptr_t)(_data), \
            (fd_t)_IOVAR_VAL(_fd), (ioseek_t)_IOVAR_VAL(_origin), (ssize_t)_IOVAR_VAL(_offset)); \
        iosqe_put(_ring); \
    })

/**
 * @brief Queue a memory map operation.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @note The reason for the redirection to the `_*_IMPL` macro is to ensure that `IOBUF()` expands correctly.
 *
 * @param _fd The file descriptor to map.
 * @param _address The virtual address to map the file into, or `NULL` for any address.
 * @param _count The number of bytes to map.
 * @param _offset The offset within the file to start mapping from.
 * @param _map Memory mapping flags.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOMAPQ(_fd, _address, _count, _offset, _map, _link, _save, _data) \
    IOMAPQT(_fd, _address, _count, _offset, _map, CLOCKS_NEVER, _link, _save, _data)

/**
 * @brief Queue a memory map operation with a timeout.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @note The reason for the redirection to the `_*_IMPL` macro is to ensure that `IOBUF()` expands correctly.
 *
 * @param _fd The file descriptor to map.
 * @param _address The virtual address to map the file into, or `NULL` for any address.
 * @param _count The number of bytes to map.
 * @param _offset The offset within the file to start mapping from.
 * @param _map Memory mapping flags.
 * @param _timeout The timeout for the operation.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOMAPQT(_fd, _address, _count, _offset, _map, _timeout, _link, _save, _data) \
    ({ \
        ioring_t* _ring = _IORING_GET(); \
        iosqe_t* _sqe = _iosqe_get(_ring); \
        ioprep_map(_sqe, _IOOPTS_FLAGS(_link, _save, _fd, _address, _count, _offset, _map), (_timeout), \
            (uintptr_t)(_data), (fd_t)_IOVAR_VAL(_fd), (void*)_IOVAR_VAL(_address), (size_t)_IOVAR_VAL(_count), \
            (ssize_t)_IOVAR_VAL(_offset), (iomap_t)_IOVAR_VAL(_map)); \
        iosqe_put(_ring); \
    })

/**
 * @brief Queue a walk operation.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @param _cwd The file descriptor to open the file relative to, or `FDCWD` to open from the current working directory.
 * @param _root The file descriptor to use as the root for the walk, or `FDROOT` to use the standard root directory.
 * @param _path The path to the file to open.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOWALKQ(_cwd, _root, _path, _link, _save, _data) IOWALKQT(_cwd, _root, _path, CLOCKS_NEVER, _link, _save, _data)

/**
 * @brief Queue a walk operation with a timeout.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @param _cwd The file descriptor to open the file relative to, or `FDCWD` to open from the current working directory.
 * @param _root The file descriptor to use as the root for the walk, or `FDROOT` to use the standard root directory.
 * @param _path The path to the file to open.
 * @param _timeout The timeout for the operation.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOWALKQT(_cwd, _root, _path, _timeout, _link, _save, _data) \
    ({ \
        ioring_t* _ring = _IORING_GET(); \
        iosqe_t* _sqe = _iosqe_get(_ring); \
        const char* _p = (const char*)_IOVAR_VAL(_path); \
        ioprep_walk(_sqe, _IOOPTS_FLAGS(_link, _save, _cwd, _root, _path, 0, 0), (_timeout), (uintptr_t)(_data), \
            (fd_t)_IOVAR_VAL(_cwd), (fd_t)_IOVAR_VAL(_root), _p, _p != NULL ? _IORING_STRLEN(_p) : 0); \
        iosqe_put(_ring); \
    })

/**
 * @brief Queue a drop operation.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @param _fd The file descriptor to drop.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IODROPQ(_fd, _link, _save, _data) IODROPQT(_fd, CLOCKS_NEVER, _link, _save, _data)

/**
 * @brief Queue a drop operation with a timeout.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @param _fd The file descriptor to drop.
 * @param _timeout The timeout for the operation.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IODROPQT(_fd, _timeout, _link, _save, _data) \
    ({ \
        ioring_t* _ring = _IORING_GET(); \
        iosqe_t* _sqe = _iosqe_get(_ring); \
        ioprep_drop(_sqe, _IOOPTS_FLAGS(_link, _save, _fd, 0, 0, 0, 0), (_timeout), (uintptr_t)(_data), \
            (fd_t)_IOVAR_VAL(_fd)); \
        iosqe_put(_ring); \
    })

/**
 * @brief Queue a remove operation.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @param _fd The file descriptor to remove.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOREMOVEQ(_fd, _link, _save, _data) IOREMOVEQT(_fd, CLOCKS_NEVER, _link, _save, _data)

/**
 * @brief Queue a remove operation with a timeout.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @param _fd The file descriptor to remove.
 * @param _timeout The timeout for the operation.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOREMOVEQT(_fd, _timeout, _link, _save, _data) \
    ({ \
        ioring_t* _ring = _IORING_GET(); \
        iosqe_t* _sqe = _iosqe_get(_ring); \
        ioprep_remove(_sqe, _IOOPTS_FLAGS(_link, _save, _fd, 0, 0, 0, 0), (_timeout), (uintptr_t)(_data), \
            (fd_t)_IOVAR_VAL(_fd)); \
        iosqe_put(_ring); \
    })

/**
 * @brief Queue an attribute operation.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @param _fd The file descriptor.
 * @param _attr The attribute to get or set.
 * @param _value The value to set (ignored for getters).
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOATTRQ(_fd, _attr, _value, _link, _save, _data) IOATTRQT(_fd, _attr, _value, CLOCKS_NEVER, _link, _save, _data)

/**
 * @brief Queue an attribute operation with a timeout.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @param _fd The file descriptor.
 * @param _attr The attribute to get or set.
 * @param _value The value to set (ignored for getters).
 * @param _timeout The timeout for the operation.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOATTRQT(_fd, _attr, _value, _timeout, _link, _save, _data) \
    ({ \
        ioring_t* _ring = _IORING_GET(); \
        iosqe_t* _sqe = _iosqe_get(_ring); \
        ioprep_attr(_sqe, _IOOPTS_FLAGS(_link, _save, _fd, _attr, _value, 0, 0), (_timeout), (uintptr_t)(_data), \
            (fd_t)_IOVAR_VAL(_fd), (file_attr_t)_IOVAR_VAL(_attr), (uint64_t)_IOVAR_VAL(_value)); \
        iosqe_put(_ring); \
    })

/**
 * @brief Queue a query operation.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @param _fd The file descriptor.
 * @param _info Pointer to the `file_info_t` structure to fill.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOQUERYQ(_fd, _info, _link, _save, _data) IOQUERYQT(_fd, _info, CLOCKS_NEVER, _link, _save, _data)

/**
 * @brief Queue a query operation with a timeout.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @param _fd The file descriptor.
 * @param _info Pointer to the `file_info_t` structure to fill.
 * @param _timeout The timeout for the operation.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOQUERYQT(_fd, _info, _timeout, _link, _save, _data) \
    ({ \
        ioring_t* _ring = _IORING_GET(); \
        iosqe_t* _sqe = _iosqe_get(_ring); \
        ioprep_query(_sqe, _IOOPTS_FLAGS(_link, _save, _fd, _info, 0, 0, 0), (_timeout), (uintptr_t)(_data), \
            (fd_t)_IOVAR_VAL(_fd), (file_info_t*)_IOVAR_VAL(_info)); \
        iosqe_put(_ring); \
    })

/**
 * @brief Queue a flush operation.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @param _fd The file descriptor to flush.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOFLUSHQ(_fd, _link, _save, _data) IOFLUSHQT(_fd, CLOCKS_NEVER, _link, _save, _data)

/**
 * @brief Queue a flush operation with a timeout.
 *
 * All arguments except `_link`, `_save` and `_data` can also be specified as a `ioreg_t`, this will utilize _Generic to
 * cause the operation to load its arguments from that register.
 *
 * @param _fd The file descriptor to flush.
 * @param _timeout The timeout for the operation.
 * @param _link The link type (e.g., `IONOLINK`, `IOSOFT`, `IOHARD`).
 * @param _save The register to save the result into, or `NULL`.
 * @param _data User data to associate with the operation.
 */
#define IOFLUSHQT(_fd, _timeout, _link, _save, _data) \
    ({ \
        ioring_t* _ring = _IORING_GET(); \
        iosqe_t* _sqe = _iosqe_get(_ring); \
        ioprep_flush(_sqe, _IOOPTS_FLAGS(_link, _save, _fd, 0, 0, 0, 0), (_timeout), (uintptr_t)(_data), \
            (fd_t)_IOVAR_VAL(_fd)); \
        iosqe_put(_ring); \
    })

/**
 * @brief Wait for an I/O completion.
 *
 * If a completion is available, it is returned immediately.
 * Otherwise, the function blocks until a completion becomes available.
 *
 * @param out Output pointer for the completion queue entry.
 * @return An appropriate status value.
 */
static inline status_t iowait(iocqe_t* out)
{
    ioring_t* ring = _IORING_GET();
    iocqe_t* cqe;
    while ((cqe = iocqe_get(ring)) == NULL)
    {
        status_t status = ioring_enter(ring, iosqe_count(ring), 1, NULL);
        if (IS_ERR(status))
        {
            return status;
        }
    }
    *out = *cqe;
    iocqe_put(ring);
    return OK;
}

/**
 * @brief Will submit all currently pending submissions and wait for all pending operations, including in flight
 * operations, to complete.
 *
 * The returned status follows a priority scheme:
 * - If a synchronous error occurs such as with `ioring_enter()` that status will be returned immediately.
 * - If no synchronous error occurs, then the status of the first completion queue entry containing an error status will
 * be returned.
 * - If no completion queue entry contains an error, then the status of the first completion queue entry containing a
 * status that is not `OK` will be returned.
 * - If none of the above are true, `OK` is returned.
 *
 * @return An appropriate status value.
 */
static inline status_t iosync(void)
{
    ioring_t* ring = _IORING_GET();
    uint32_t stail = atomic_load_explicit(&ring->ctrl->stail, memory_order_relaxed);
    uint32_t chead = atomic_load_explicit(&ring->ctrl->chead, memory_order_relaxed);
    uint32_t pending = stail - chead;

    status_t status = OK;
    while (pending > 0)
    {
        status_t status2 =
            ioring_enter(ring, iosqe_count(ring), pending > ring->centries ? ring->centries : pending, NULL);
        if (IS_ERR(status2))
        {
            return status2;
        }

        iocqe_t* cqe;
        while (pending > 0 && (cqe = iocqe_get(ring)) != NULL)
        {
            if ((IS_ERR(cqe->status) && IS_INFO(status)) || (status == OK && cqe->status != OK))
            {
                status = cqe->status;
            }
            iocqe_put(ring);
            pending--;
        }
    }

    return status;
}

/**
 * @brief Synchronous wrapper for a read operation with timeout.
 *
 * @param fd The file descriptor to read from.
 * @param vector An array of `iovec_t` structures to read into.
 * @param count The number of `iovec_t` structures.
 * @param offset The offset to read from, or `IOCUR`.
 * @param timeout Timeout for the operation, `CLOCKS_NEVER` for no timeout or `CLOCKS_NOW` to fail the operation if it
 * cannot be completed immediately.
 * @param bytesRead Output pointer for the number of bytes read, can be `NULL`.
 * @return An appropriate status value.
 */
static inline status_t ioreadt(fd_t fd, const iovec_t* vector, size_t count, ssize_t offset, clock_t timeout,
    size_t* bytesRead)
{
    IOREADQT(fd, vector, count, offset, timeout, IONOLINK, NULL, 0);
    iocqe_t cqe;
    status_t status = iowait(&cqe);
    if (IS_ERR(status))
    {
        return status;
    }
    if (bytesRead != NULL)
    {
        *bytesRead = cqe.result;
    }
    return cqe.status;
}

/**
 * @brief Synchronous wrapper for a read operation.
 *
 * @param fd The file descriptor to read from.
 * @param vector An array of `iovec_t` structures to read into.
 * @param count The number of `iovec_t` structures.
 * @param offset The offset to read from, or `IOCUR`.
 * @param bytesRead Output pointer for the number of bytes read, can be `NULL`.
 * @return An appropriate status value.
 */
static inline status_t ioread(fd_t fd, const iovec_t* vector, size_t count, ssize_t offset, size_t* bytesRead)
{
    return ioreadt(fd, vector, count, offset, CLOCKS_NEVER, bytesRead);
}

/**
 * @brief Synchronous wrapper for a write operation with timeout.
 *
 * @param fd The file descriptor to write to.
 * @param vector An array of `iovec_t` structures to write from.
 * @param count The number of `iovec_t` structures.
 * @param offset The offset to write to, or `IOCUR`.
 * @param timeout Timeout for the operation, `CLOCKS_NEVER` for no timeout or `CLOCKS_NOW` to fail the operation if it
 * cannot be completed immediately.
 * @param bytesWritten Output pointer for the number of bytes written, can be `NULL`.
 * @return An appropriate status value.
 */
static inline status_t iowritet(fd_t fd, const iovec_t* vector, size_t count, ssize_t offset, clock_t timeout,
    size_t* bytesWritten)
{
    IOWRITEQT(fd, vector, count, offset, timeout, IONOLINK, NULL, 0);
    iocqe_t cqe;
    status_t status = iowait(&cqe);
    if (IS_ERR(status))
    {
        return status;
    }
    if (bytesWritten != NULL)
    {
        *bytesWritten = cqe.result;
    }
    return cqe.status;
}

/**
 * @brief Synchronous wrapper for a write operation.
 *
 * @param fd The file descriptor to write to.
 * @param vector An array of `iovec_t` structures to write from.
 * @param count The number of `iovec_t` structures.
 * @param offset The offset to write to, or `IOCUR`.
 * @param bytesWritten Output pointer for the number of bytes written, can be `NULL`.
 * @return An appropriate status value.
 */
static inline status_t iowrite(fd_t fd, const iovec_t* vector, size_t count, ssize_t offset, size_t* bytesWritten)
{
    return iowritet(fd, vector, count, offset, CLOCKS_NEVER, bytesWritten);
}

/**
 * @brief Synchronous wrapper for reading a file into a null-terminated string.
 *
 * @param fd The file descriptor to read from.
 * @param out Output pointer for the null-terminated string.
 * @param outLen Output pointer for the length of the string.
 * @return An appropriate status value.
 */
status_t ioload(fd_t fd, char** out, size_t* outLen);

/**
 * @brief Synchronous wrapper for writing a null-terminated string to a file.
 *
 * @param fd The file descriptor to write from.
 * @param in The null-terminated string to write.
 * @param bytesWritten Output pointer for the number of bytes written, can be `NULL`.
 * @return An appropriate status value.
 */
status_t iostore(fd_t fd, const char* in, size_t* bytesWritten);

/**
 * @brief Synchronous wrapper for a poll operation.
 *
 * @param fd The file descriptor to poll.
 * @param events The events to wait for.
 * @param timeout Timeout for the operation, `CLOCKS_NEVER` for no timeout or `CLOCKS_NOW` to fail the operation if it
 * cannot be completed immediately.
 * @param revents Output pointer for the events that occurred, can be `NULL`.
 * @return An appropriate status value.
 */
static inline status_t iopoll(fd_t fd, ioevents_t events, clock_t timeout, ioevents_t* revents)
{
    IOPOLLQT(fd, events, timeout, IONOLINK, NULL, 0);
    iocqe_t cqe;
    status_t status = iowait(&cqe);
    if (IS_ERR(status))
    {
        return status;
    }
    if (revents != NULL)
    {
        *revents = (ioevents_t)cqe.result;
    }
    return cqe.status;
}

/**
 * @brief Synchronous wrapper for polling multiple files.
 *
 * @param fds Array of pollfd structures.
 * @param nfds Number of file descriptors.
 * @param timeout Timeout in clock ticks.
 * @param count Output pointer for the number of events.
 * @return An appropriate status value.
 */
status_t iopolln(iopoll_t* fds, size_t nfds, clock_t timeout, size_t* count);

/**
 * @brief Synchronous wrapper for a seek operation with a timeout.
 *
 * @param fd The file descriptor to seek.
 * @param origin The origin of the seek operation.
 * @param offset The offset to seek to.
 * @param timeout Timeout for the operation.
 * @param pos Output pointer for the new file position, can be `NULL`.
 * @return An appropriate status value.
 */
static inline status_t ioseekt(fd_t fd, ioseek_t origin, ssize_t offset, clock_t timeout, size_t* pos)
{
    IOSEEKQT(fd, origin, offset, timeout, IONOLINK, NULL, 0);
    iocqe_t cqe;
    status_t status = iowait(&cqe);
    if (IS_ERR(status))
    {
        return status;
    }
    if (pos != NULL)
    {
        *pos = (size_t)cqe.result;
    }
    return cqe.status;
}

/**
 * @brief Synchronous wrapper for a seek operation.
 *
 * @param fd The file descriptor to seek.
 * @param origin The origin of the seek operation.
 * @param offset The offset to seek to.
 * @param pos Output pointer for the new file position, can be `NULL`.
 * @return An appropriate status value.
 */
static inline status_t ioseek(fd_t fd, ioseek_t origin, ssize_t offset, size_t* pos)
{
    return ioseekt(fd, origin, offset, CLOCKS_NEVER, pos);
}

/**
 * @brief Synchronous wrapper for a memory map operation with a timeout.
 *
 * @param fd The file descriptor to map.
 * @param address Input/Output pointer for the virtual address, if *address is NULL the kernel will choose an
 * address.
 * @param count The number of bytes to map.
 * @param offset The offset within the file to start mapping from.
 * @param mem Memory mapping flags.
 * @param timeout Timeout for the operation.
 * @return An appropriate status value.
 */
static inline status_t iomapt(fd_t fd, void** address, size_t count, ssize_t offset, iomap_t mem, clock_t timeout)
{
    if ((void*)address == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    IOMAPQT(fd, *address, count, offset, mem, timeout, IONOLINK, NULL, 0);
    iocqe_t cqe;
    status_t status = iowait(&cqe);
    if (IS_ERR(status))
    {
        return status;
    }
    *address = (void*)cqe.result;
    return cqe.status;
}

/**
 * @brief Synchronous wrapper for a memory map operation.
 *
 * @param fd The file descriptor to map.
 * @param address Input/Output pointer for the virtual address, if *address is NULL the kernel will choose an
 * address.
 * @param count The number of bytes to map.
 * @param offset The offset within the file to start mapping from.
 * @param mem Memory mapping flags.
 * @return An appropriate status value.
 */
static inline status_t iomap(fd_t fd, void** address, size_t count, ssize_t offset, iomap_t mem)
{
    return iomapt(fd, address, count, offset, mem, CLOCKS_NEVER);
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
    return syscall2(SYS_UNMAP, NULL, (uintptr_t)address, length);
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
static inline status_t ioprotect(void* address, size_t length, iomap_t map)
{
    return syscall3(SYS_PROTECT, NULL, (uintptr_t)address, length, map);
}

/**
 * @brief Synchronous wrapper for an walk operation with timeout.
 *
 * @param cwd The file descriptor to start walking from, or `FDCWD` start at the current working directory.
 * @param root The file descriptor to use as the root for the walk, or `FDROOT` to use the standard root directory.
 * @param path The path to walk.
 * @param timeout Timeout for the operation, `CLOCKS_NEVER` for no timeout or `CLOCKS_NOW` to fail the operation if it
 * cannot be completed immediately.
 * @param opened Output pointer for the opened file descriptor.
 * @return An appropriate status value.
 */
static inline status_t iowalkt(fd_t cwd, fd_t root, const char* path, clock_t timeout, fd_t* opened)
{
    IOWALKQT(cwd, root, path, timeout, IONOLINK, NULL, 0);
    iocqe_t cqe;
    status_t status = iowait(&cqe);
    if (IS_ERR(status))
    {
        return status;
    }
    *opened = cqe.result;
    return cqe.status;
}

/**
 * @brief Synchronous wrapper for an walk operation.
 *
 * @param cwd The file descriptor to start walking from, or `FDCWD` start at the current working directory.
 * @param root The file descriptor to use as the root for the walk, or `FDROOT` to use the standard root directory.
 * @param path The path to walk.
 * @param opened Output pointer for the opened file descriptor.
 * @return An appropriate status value.
 */
static inline status_t iowalk(fd_t cwd, fd_t root, const char* path, fd_t* opened)
{
    return iowalkt(cwd, root, path, CLOCKS_NEVER, opened);
}

/**
 * @brief Synchronous wrapper for a drop operation with timeout.
 *
 * @param fd The file descriptor to drop.
 * @param timeout Timeout for the operation.
 * @return An appropriate status value.
 */
static inline status_t iodropt(fd_t fd, clock_t timeout)
{
    IODROPQT(fd, timeout, IONOLINK, NULL, 0);
    iocqe_t cqe;
    status_t status = iowait(&cqe);
    if (IS_ERR(status))
    {
        return status;
    }
    return cqe.status;
}

/**
 * @brief Synchronous wrapper for a drop operation.
 *
 * @param fd The file descriptor to drop.
 * @return An appropriate status value.
 */
static inline status_t iodrop(fd_t fd)
{
    return iodropt(fd, CLOCKS_NEVER);
}

/**
 * @brief Synchronous wrapper for a remove operation with timeout.
 *
 * @param fd The file descriptor to remove.
 * @param timeout Timeout for the operation.
 * @return An appropriate status value.
 */
static inline status_t ioremovet(fd_t fd, clock_t timeout)
{
    IOREMOVEQT(fd, timeout, IONOLINK, NULL, 0);
    iocqe_t cqe;
    status_t status = iowait(&cqe);
    if (IS_ERR(status))
    {
        return status;
    }
    return cqe.status;
}

/**
 * @brief Synchronous wrapper for a remove operation.
 *
 * @param fd The file descriptor to remove.
 * @return An appropriate status value.
 */
static inline status_t ioremove(fd_t fd)
{
    return ioremovet(fd, CLOCKS_NEVER);
}

/**
 * @brief Synchronous wrapper for an attribute operation with timeout.
 *
 * @param fd The file descriptor.
 * @param attr The attribute to get or set (e.g., IOATTR_GET_SIZE, IOATTR_SET_SIZE).
 * @param value Pointer to the value to set or retrieve.
 * @param timeout Timeout for the operation.
 * @result The requested value or `0` if setting a value.
 */
static inline status_t ioattrt(fd_t fd, file_attr_t attr, uint64_t* value, clock_t timeout)
{
    IOATTRQT(fd, attr, *value, timeout, IONOLINK, NULL, 0);
    iocqe_t cqe;
    status_t status = iowait(&cqe);
    if (IS_ERR(status))
    {
        return status;
    }
    *value = cqe.result;
    return cqe.status;
}

/**
 * @brief Synchronous wrapper for an attribute operation.
 *
 * @param fd The file descriptor.
 * @param attr The attribute to get or set (e.g., IOATTR_GET_SIZE, IOATTR_SET_SIZE).
 * @param value Pointer to the value to set or retrieve.
 * @result The requested value or `0` if setting a value.
 */
static inline status_t ioattr(fd_t fd, file_attr_t attr, uint64_t* value)
{
    return ioattrt(fd, attr, value, CLOCKS_NEVER);
}

/**
 * @brief Synchronous wrapper for a query operation with timeout.
 *
 * @param fd The file descriptor.
 * @param info Pointer to the `file_info_t` structure to fill.
 * @param timeout Timeout for the operation.
 * @result The status of the operation.
 */
static inline status_t ioqueryt(fd_t fd, file_info_t* info, clock_t timeout)
{
    IOQUERYQT(fd, info, timeout, IONOLINK, NULL, 0);
    iocqe_t cqe;
    status_t status = iowait(&cqe);
    if (IS_ERR(status))
    {
        return status;
    }
    return cqe.status;
}

/**
 * @brief Synchronous wrapper for a query operation.
 *
 * @param fd The file descriptor.
 * @param info Pointer to the `file_info_t` structure to fill.
 * @result The status of the operation.
 */
static inline status_t ioquery(fd_t fd, file_info_t* info)
{
    return ioqueryt(fd, info, CLOCKS_NEVER);
}

/**
 * @brief Synchronous wrapper for a flush operation with timeout.
 *
 * @param fd The file descriptor to flush.
 * @param timeout Timeout for the operation.
 * @return An appropriate status value.
 */
static inline status_t ioflusht(fd_t fd, clock_t timeout)
{
    IOFLUSHQT(fd, timeout, IONOLINK, NULL, 0);
    iocqe_t cqe;
    status_t status = iowait(&cqe);
    if (IS_ERR(status))
    {
        return status;
    }
    return cqe.status;
}

/**
 * @brief Synchronous wrapper for a flush operation.
 *
 * @param fd The file descriptor to flush.
 * @return An appropriate status value.
 */
static inline status_t ioflush(fd_t fd)
{
    return ioflusht(fd, CLOCKS_NEVER);
}

/**
 * @brief Synchronous wrapper for a reading a file directly using a path.
 *
 * This wrapper is more efficient than calling `iowalk()`, `ioread()`/`iowrite()`, and `iodrop()` in sequence as it
 * uses the register system to chain the operations into a single `ioring_enter()` call.
 *
 * @param cwd The file descriptor to start walking from, or `FDCWD` start at the current working directory.
 * @param root The file descriptor to use as the root for the walk, or `FDROOT` to use the standard root directory.
 * @param path The path to the file.
 * @param vector An array of `iovec_t` structures.
 * @param count The number of `iovec_t` structures.
 * @param offset The offset to read from, or `IOCUR`.
 * @param bytesRead Output pointer for the number of bytes read.
 * @return An appropriate status value.
 */
status_t ioreadp(fd_t cwd, fd_t root, const char* path, const iovec_t* vector, size_t count, ssize_t offset,
    size_t* bytesRead);

/**
 * @brief Synchronous wrapper for writing to a file directly using a path.
 *
 * This wrapper is more efficient than calling `iowalk()`, `ioread()`/`iowrite()`, and `iodrop()` in sequence as it
 * uses the register system to chain the operations into a single `ioring_enter()` call.
 *
 * @param cwd The file descriptor to start walking from, or `FDCWD` start at the current working directory.
 * @param root The file descriptor to use as the root for the walk, or `FDROOT` to use the standard root directory.
 * @param path The path to the file.
 * @param vector An array of `iovec_t` structures.
 * @param count The number of `iovec_t` structures.
 * @param offset The offset to write from, or `IOCUR`.
 * @param bytesWritten Output pointer for the number of bytes written.
 * @return An appropriate status value.
 */
status_t iowritep(fd_t cwd, fd_t root, const char* path, const iovec_t* vector, size_t count, ssize_t offset,
    size_t* bytesWritten);

/**
 * @brief Synchronous wrapper for reading a file directly into a null-terminated string using a path.
 *
 * @param cwd The file descriptor to start walking from, or `FDCWD` start at the current working directory.
 * @param root The file descriptor to use as the root for the walk, or `FDROOT` to use the standard root directory.
 * @param path The path to the file.
 * @param out Output pointer for the null-terminated string.
 * @param outLen Output pointer for the length of the string.
 * @return An appropriate status value.
 */
status_t ioloadp(fd_t cwd, fd_t root, const char* path, char** out, size_t* outLen);

/**
 * @brief Synchronous wrapper for writing a null-terminated string directly to a file using a path.
 *
 * @param cwd The file descriptor to start walking from, or `FDCWD` start at the current working directory.
 * @param root The file descriptor to use as the root for the walk, or `FDROOT` to use the standard root directory.
 * @param path The path to the file.
 * @param in The null-terminated string to write.
 * @return An appropriate status value.
 */
status_t iostorep(fd_t cwd, fd_t root, const char* path, const char* in);

/**
 * @brief Synchronous wrapper for removing a file directly using a path.
 *
 * @param cwd The file descriptor to start walking from, or `FDCWD` start at the current working directory.
 * @param root The file descriptor to use as the root for the walk, or `FDROOT` to use the standard root directory.
 * @param path The path to the file.
 * @return An appropriate status value.
 */
status_t ioremovep(fd_t cwd, fd_t root, const char* path);

/**
 * @brief Synchronous wrapper for getting/setting attributes of a file directly using a path.
 *
 * @param cwd The file descriptor to start walking from, or `FDCWD` start at the current working directory.
 * @param root The file descriptor to use as the root for the walk, or `FDROOT` to use the standard root directory.
 * @param path The path to the file.
 * @param attr The attribute to get or set.
 * @param value Pointer to the value to set or retrieve.
 * @return An appropriate status value.
 */
status_t ioattrp(fd_t cwd, fd_t root, const char* path, file_attr_t attr, uint64_t* value);

/**
 * @brief Synchronous wrapper for querying a file directly using a path.
 *
 * @param cwd The file descriptor to start walking from, or `FDCWD` start at the current working directory.
 * @param root The file descriptor to use as the root for the walk, or `FDROOT` to use the standard root directory.
 * @param path The path to the file.
 * @param info Pointer to the `file_info_t` structure to fill.
 * @return An appropriate status value.
 */
status_t ioqueryp(fd_t cwd, fd_t root, const char* path, file_info_t* info);

/**
 * @brief Synchronous wrapper for memory mapping a file directly using a path.
 *
 * @param cwd The file descriptor to start walking from, or `FDCWD` start at the current working directory.
 * @param root The file descriptor to use as the root for the walk, or `FDROOT` to use the standard root directory.
 * @param path The path to the file.
 * @param address Input/Output pointer for the virtual address.
 * @param count The number of bytes to map.
 * @param offset The offset within the file to start mapping from.
 * @param mem Memory mapping flags.
 * @return An appropriate status value.
 */
status_t iomapp(fd_t cwd, fd_t root, const char* path, void** address, size_t count, ssize_t offset, iomap_t mem);

/**
 * @brief Synchronous wrapper for scaning the contents of a file with a `va_list`.
 *
 * @param fd The file descriptor to scan.
 * @param count The number of bytes to scan.
 * @param offset The offset to scan from, or `IOCUR`.
 * @param matches Output pointer for the number of matches in the format, can be `NULL`.
 * @param format The format string.
 * @param args The `va_list` of arguments.
 * @return An appropriate status value.
 */
status_t ioscanv(fd_t fd, size_t count, size_t offset, uint32_t* matches, const char* format, va_list args);

/**
 * @brief Synchronous wrapper for scaning the contents of a file.
 *
 * @param fd The file descriptor to scan.
 * @param count The number of bytes to scan.
 * @param offset The offset to scan from, or `IOCUR`.
 * @param matches Output pointer for the number of matches in the format, can be `NULL`.
 * @param format The format string.
 * @param ... The arguments to be scanned into.
 * @return An appropriate status value.
 */
static inline status_t ioscan(fd_t fd, size_t count, size_t offset, uint32_t* matches, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    status_t status = ioscanv(fd, count, offset, matches, format, args);
    va_end(args);
    return status;
}

/**
 * @brief Synchronous wrapper for scaning the contents of a file using a path with a `va_list`.
 *
 * @param cwd The file descriptor to start walking from, or `FDCWD` start at the current working directory.
 * @param root The file descriptor to use as the root for the walk, or `FDROOT` to use the standard root directory.
 * @param path The path to the file.
 * @param count The number of bytes to scan.
 * @param offset The offset to scan from, or `IOCUR`.
 * @param matches Output pointer for the number of matches in the format, can be `NULL`.
 * @param format The format string.
 * @param args The `va_list` of arguments.
 * @return An appropriate status value.
 */
status_t ioscanvp(fd_t cwd, fd_t root, const char* path, size_t count, size_t offset, uint32_t* matches,
    const char* format, va_list args);

/**
 * @brief Synchronous wrapper for scaning the contents of a file using a path.
 *
 * @param cwd The file descriptor to start walking from, or `FDCWD` start at the current working directory.
 * @param root The file descriptor to use as the root for the walk, or `FDROOT` to use the standard root directory.
 * @param path The path to the file.
 * @param count The number of bytes to scan.
 * @param offset The offset to scan from, or `IOCUR`.
 * @param matches Output pointer for the number of matches in the format, can be `NULL`.
 * @param format The format string.
 * @param ... The arguments to be scanned into.
 * @return An appropriate status value.
 */
static inline status_t ioscanp(fd_t cwd, fd_t root, const char* path, size_t count, size_t offset, uint32_t* matches,
    const char* format, ...)
{
    va_list args;
    va_start(args, format);
    status_t status = ioscanvp(cwd, root, path, count, offset, matches, format, args);
    va_end(args);
    return status;
}

#endif

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
