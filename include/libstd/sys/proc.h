#ifndef _SYS_PROC_H
#define _SYS_PROC_H 1

#include <stdatomic.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_internal/ERR.h"
#include "_internal/NULL.h"
#include "_internal/clock_t.h"
#include "_internal/config.h"
#include "_internal/fd_t.h"
#include "_internal/pid_t.h"
#include "_internal/tid_t.h"

/**
 * @brief Process management header.
 * @ingroup libstd
 * @defgroup libstd_sys_proc Process management
 *
 * The `sys/proc.h` header handles process management, including process creation, managing a processes address space,
 * scheduling, and similar.
 *
 * @{
 */

/**
 * @brief Priority type.
 * @typedef priority_t
 *
 * The `priority_t` type is used to store the scheduling priority of a process, we also define three constants
 * `PRIORITY_MIN`, which represents the lowest priority a process can have, `PRIORITY_MAX` which defines the maximum
 * value of a process priority (not inclusive) and `PRIORITY_MAX_USER` which defines the maximum value that user space
 * is allowed to specify for a process (not inclusive). See the kernel `sched_schedule()` function for more info.
 *
 */
typedef uint8_t priority_t;
#define PRIORITY_MAX 64
#define PRIORITY_MAX_USER 32
#define PRIORITY_MIN 0

/**
 * @brief Stucture used to duplicate fds in `spawn()`.
 *
 * The `spawn_fd_t` structure is used to inform the kernel of which file descriptors to duplicate when spawning a new
 * process, for more information check the `spawn()` function. The special value `SPAWN_FD_END` is used to terminate
 * arrays of `spawn_fd_t` structures.
 *
 */
typedef struct
{
    fd_t child;  //!< The destination file descriptor in the child
    fd_t parent; //!< The source file descriptor in the parent
} spawn_fd_t;

/**
 * @brief Spawn behaviour flags.
 *
 * The `spawn_flags_t` type is used to modify the behaviour when spawning a new process or provide additional
 * information. We use a 64 bit integer to allow more flags to be implemented. For more information check the `spawn()`
 * function.
 *
 * Available flags:
 *
 * - `SPAWN_NONE` - None
 * - `SPAWN_INHERIT_PRIORITY` - Causes the new process to inherit the priority of the parent, the spawn_attr_t::priority
 * field is ignored.
 *
 */
typedef uint64_t spawn_flags_t;
#define SPAWN_NONE 0
#define SPAWN_INHERIT_PRIORITY (1 << 0)

/**
 * @brief Spawn fds termination constant.
 *
 */
#define SPAWN_FD_END \
    (spawn_fd_t) \
    { \
        .child = FD_NONE, .parent = FD_NONE \
    }

typedef struct
{
    spawn_flags_t flags;
    priority_t priority;
    uint8_t _padding[128 - sizeof(priority_t) - sizeof(spawn_flags_t)];
} spawn_attr_t;

#ifdef static_assert
static_assert(sizeof(spawn_attr_t) == 128);
#endif

/**
 * @brief System call for creating child processes.
 *
 * @param argv A NULL-terminated array of strings, where `argv[0]` is the filepath to the desired executable. This array
 * will be pushed to the child stack and the child can find a pointer to this array in its rsi register, along with its
 * length in the rdi register.
 * @param fds A array of file descriptors to be duplicated to the child process. Each `spawn_fd_t` in the array
 * specifies a source file descriptor in the parent (`.parent`) and its destination in the child (`.child`).
 * The array must be terminated by `SPAWN_FD_END`.
 * @param cwd The working directory for the child process. If `NULL`, the child inherits the parent's current
 * working directory.
 * @param attr The spawn attributes, allows for specifying additional information for the new process, if equal to NULL
 * then use defaults.
 * @return On success, returns the childs pid, on failure returns `ERR` and errno is set.
 */
pid_t spawn(const char** argv, const spawn_fd_t* fds, const char* cwd, spawn_attr_t* attr);

/**
 * @brief System call to retrieve the current pid.
 *
 * The `getpid()` function retrieves the pid of the currently running process.
 *
 * @return The running processes pid.
 */
pid_t getpid(void);

/**
 * @brief System call to retrieve the current tid.
 *
 * The `gettid()` function retrieves the tid of the currently running thread.
 *
 * @return The running threads tid.
 */
tid_t gettid(void);

/**
 * @brief Memory page size
 *
 * The `PAGE_SIZE` constant stores the size in bytes of one page in memory.
 *
 */
#define PAGE_SIZE 0x1000

/**
 * @brief Convert bytes to pages.
 *
 * The `BYTES_TO_PAGES()` macro takes in a amount of bytes and returns the amount of pages in memory required to store
 * that amount of bytes.
 *
 * @param amount The amount of bytes.
 * @return The amount of pages.
 */
#define BYTES_TO_PAGES(amount) (((amount) + PAGE_SIZE - 1) / PAGE_SIZE)

/**
 * @brief Size of an object in pages.
 *
 * The `PAGE_SIZE_OF()` macro returns the amount of pages in memory required to store a given object.
 *
 * @param object The object to calculate the page size of.
 * @return The amount of pages.
 */
#define PAGE_SIZE_OF(object) BYTES_TO_PAGES(sizeof(object))

/**
 * @brief Memory protection flags.
 *
 * The `prot_t` enum is used to store the memory protection flags of a region in memory.
 *
 */
typedef enum
{
    PROT_NONE = 0,        //!< None
    PROT_READ = (1 << 0), //!< Memory can be read from
    PROT_WRITE = (1 << 1) //!< Memory can be written to
} prot_t;

/**
 * @brief System call to map memory from a file.
 *
 * The `mmap()` function maps memory to the currently running processes address space from a file, this is the only way
 * to allocate virtual memory from userspace. An example usage would be to map the `/dev/zero` file which would allocate
 * zeroed memory.
 *
 * @param fd The open file descriptor of the file to be mapped.
 * @param address The desired virtual destination address, if equal to `NULL` the kernel will choose a available
 * address, will be rounded down to the nearest page multiple.
 * @param length The length of the segment to be mapped, note that this length will be rounded up to the nearest page
 * multiple by the kernel factoring in page boundaries.
 * @param prot Protection flags.
 * @return On success, returns the address of the mapped memory, will always be page aligned, on failure returns `NULL`
 * and errno is set.
 */
void* mmap(fd_t fd, void* address, uint64_t length, prot_t prot);

/**
 * @brief System call to unmap mapped memory.
 *
 * The `munmap()` function unmaps memory from the currently running processes address space.
 *
 * @param address The starting virtual address of the memory area to be unmapped.
 * @param length The length of the memory area to be unmapped.
 * @return On success, returns 0, on failure returns `ERR` and errno is set.
 */
uint64_t munmap(void* address, uint64_t length);

/**
 * @brief System call to change the protection flags of memory.
 *
 * The `mprotect()` changes the protection flags of a virtual memory area in the currently running processes address
 * space.
 *
 * @param address  The starting virtual address of the memory area to be modified.
 * @param length The length of the memory area to be modifed.
 * @param prot The new protection flags of the memory area.
 * @return On success, returns 0, on failure returns `ERR` and errno is set.
 */
uint64_t mprotect(void* address, uint64_t length, prot_t prot);

/**
 * @brief Futex operation enum.
 *
 * The `futex_op_t` enum is used to specify the desired futex operation in the `futex()` function.
 *
 */
typedef enum
{
    FUTEX_WAIT, //!< The futex operating for waiting until the value pointed to by `addr` is not equal to `val`.
    FUTEX_WAKE, //!< The futex operation for waking up a amount of threads specified by the `val` argument.
} futex_op_t;

/**
 * @brief Futex wake all constant.
 *
 * The `FUTEX_ALL` constant can be used as the `val` argument when using the `FUTEX_WAIT` operating in the `futex()`
 * function to wake upp all waiters.
 *
 */
#define FUTEX_ALL UINT64_MAX

/**
 * @brief System call for fast user space mutual exclusion.
 *
 * The `futex()` function provides a fast user-space syncronization mechanism. It can be used to implement userspace
 * mutexes, conditional variables, etc.
 *
 * @param addr A pointer to an atomic 64-bit unsigned integer.
 * @param val The value to compare against for `FUTEX_WAIT` or the number of threads to wake for `FUTEX_WAKE`.
 * @param op The futex operation to perform (e.g., `FUTEX_WAIT` or `FUTEX_WAKE`).
 * @param timeout An optional timeout for `FUTEX_WAIT`. If `CLOCKS_NEVER`, it waits forever.
 * @return On success, returns 0, except if using the `FUTEX_WAKE` operation then it returns the number of woken
 * threads. On failure, returns `ERR` and errno is set.
 */
uint64_t futex(atomic_uint64_t* addr, uint64_t val, futex_op_t op, clock_t timeout);

/**
 * @brief System call for retreving the time since boot.
 *
 * The `uptime()` function retrieves the system uptime since boot in clock ticks.
 *
 * @return The system uptime in clock ticks.
 */
clock_t uptime(void);

/**
 * @brief System call for sleeping.
 *
 * The `nanosleep()` function suspends the execution of the calling thread for a specified duration.
 *
 * @param timeout The duration in nanoseconds for which to sleep, if equal to `CLOCKS_NEVER` then it will sleep forever,
 * not sure why you would want to do that but you can.
 * @return On success, returns 0. On failure, returns `ERR` and errno is set.
 */
uint64_t nanosleep(clock_t timeout);

/**
 * @brief Synchronization object.
 *
 * The `sync_t` structure is used to implement user space synchronization primitives. Its the object mapped when calling
 * 'mmap()' on a opened sync file. For more information check the `sync.h` header.
 *
 * @see sync.h
 */
typedef struct
{
    atomic_uint64_t value; //!< The value of the sync object.
} sync_t;

#if defined(__cplusplus)
}
#endif

#endif

/** @} */
