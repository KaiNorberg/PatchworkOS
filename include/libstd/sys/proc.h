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
 * The `sys/proc.h` header handles process management, including process spawning, managing a processes address space,
 * scheduling, and similar.
 *
 * @{
 */

/**
 * @brief The environment variables of the current process.
 *
 * The `environ` variable is a NULL-terminated array of strings representing the environment variables of the current
 * process in the format "KEY=VALUE".
 */
extern char** environ;

/**
 * @brief Priority type.
 * @typedef priority_t
 *
 * The `priority_t` type is used to store the scheduling priority of a process.
 *
 */
typedef uint8_t priority_t;

#define PRIORITY_PARENT 255  ///< Use the priority of the parent process.
#define PRIORITY_MAX 63      ///< The maximum priority value, inclusive.
#define PRIORITY_MAX_USER 31 ///< The maximum priority user space is allowed to specify, inclusive.
#define PRIORITY_MIN 0       ///< The minimum priority value.

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
    fd_t child;  ///< The destination file descriptor in the child
    fd_t parent; ///< The source file descriptor in the parent
} spawn_fd_t;

/**
 * @brief Spawn behaviour flags.
 * @enum spawn_flags_t
 */
typedef enum
{
    SPAWN_DEFAULT = 0,               ///< Default spawn behaviour.
    SPAWN_EMPTY_NAMESPACE = 1 << 0, ///< Dont inherit the mountpoints of the parent's namespace.
    SPAWN_EMPTY_ENVIRONMENT = 1 << 1, ///< Don't inherit the parent's environment.
    /**
     * Starts the spawned process in a suspended state. The process will not begin executing until a "continue" note is received.
     * 
     * The purpose of this flag is to allow the parent process to modify the child process before it starts executing, for example modifying its environment variables.
     * 
     * @todo Starting a process suspended is not yet implemented.
     */
    SPAWN_START_SUSPENDED = 1 << 2 
} spawn_flags_t;

/**
 * @brief Spawn fds termination constant.
 */
#define SPAWN_FD_END \
    (spawn_fd_t) \
    { \
        .child = FD_NONE, .parent = FD_NONE \
    }

/**
 * @brief System call for spawning new processes.
 *
 * @param argv A NULL-terminated array of strings, where `argv[0]` is the filepath to the desired executable.
 * @param fds A array of file descriptors to be duplicated to the child process. Each `spawn_fd_t` in the array
 * specifies a source file descriptor in the parent (`.parent`) and its destination in the child (`.child`).
 * The array must be terminated by `SPAWN_FD_END`.
 * @param cwd The working directory for the child process, or `NULL` to inherit the parents working directory.
 * @param priority The scheduling priority for the child process, or `PRIORITY_PARENT` to inherit the parent's priority.
 * @param flags Spawn behaviour flags.
 * @return On success, the childs pid. On failure, `ERR` and `errno` is set.
 */
pid_t spawn(const char** argv, const spawn_fd_t* fds, const char* cwd, priority_t priority, spawn_flags_t flags);

/**
 * @brief System call to retrieve the current pid.
 *
 * @return The running processes pid.
 */
pid_t getpid(void);

/**
 * @brief System call to retrieve the current tid.
 *
 * @return The running threads tid.
 */
tid_t gettid(void);

/**
 * @brief The size of a memory page in bytes.
 */
#define PAGE_SIZE 0x1000

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
 * @brief Memory protection flags.
 * @typedef prot_t
 */
typedef enum
{
    PROT_NONE = 0,          ///< Invalid memory, cannot be accessed.
    PROT_READ = (1 << 0),   ///< Readable memory.
    PROT_WRITE = (1 << 1),  ///< Writable memory.
    PROT_EXECUTE = (1 << 2) ///< Executable memory.
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
 * @param prot Protection flags, must have at least `PROT_READ` set.
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
 * @return On success, returns the address of the unmapped memory, on failure returns `NULL` and errno is set.
 */
void* munmap(void* address, uint64_t length);

/**
 * @brief System call to change the protection flags of memory.
 *
 * The `mprotect()` changes the protection flags of a virtual memory area in the currently running processes address
 * space.
 *
 * @param address  The starting virtual address of the memory area to be modified.
 * @param length The length of the memory area to be modifed.
 * @param prot The new protection flags of the memory area, if equal to `PROT_NONE` the memory area will be
 * unmapped.
 * @return On success, returns the address of the modified memory area, on failure returns `NULL` and errno is set.
 */
void* mprotect(void* address, uint64_t length, prot_t prot);

/**
 * @brief Futex operation enum.
 *
 * The `futex_op_t` enum is used to specify the desired futex operation in the `futex()` function.
 *
 */
typedef enum
{
    /**
     * @brief Wait until the timeout expires or the futex value changes.
     * 
     * If the value at the futex address is not equal to `val`, the call returns immediately with `EAGAIN`.
     * Otherwise, the calling thread is put to sleep until another thread wakes it up or the specified timeout expires.
     */
    FUTEX_WAIT,
    /**
     * @brief Wake up one or more threads waiting on the futex.
     * 
     * Wakes up a maximum of `val` number of threads that are currently waiting on the futex at the specified address. If `val` is `FUTEX_ALL`, all waiting threads are woken up.
     */
    FUTEX_WAKE,
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
 * @param val The value used by the futex operation, its meaning depends on the operation.
 * @param op The futex operation to perform (e.g., `FUTEX_WAIT` or `FUTEX_WAKE`).
 * @param timeout An optional timeout for `FUTEX_WAIT`. If `CLOCKS_NEVER`, it waits forever.
 * @return On success, depends on the operation. On failure, `ERR` and errno is set.
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
 * @return On success, `0`. On failure, `ERR` and errno is set.
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
    atomic_uint64_t value; ///< The value of the sync object.
} sync_t;

#if defined(__cplusplus)
}
#endif

#endif

/** @} */
