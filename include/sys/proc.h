#ifndef _SYS_PROC_H
#define _SYS_PROC_H 1

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/syscall.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libstd/_FAIL.h"
#include "_libstd/NULL.h"
#include "_libstd/PAGE_SIZE.h"
#include "_libstd/clock_t.h"
#include "_libstd/config.h"
#include "_libstd/fd_t.h"

#include "_libstd/pid_t.h"
#include "_libstd/tid_t.h"

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
 * @brief Priority type.
 * @typedef priority_t
 *
 * The `priority_t` type is used to store the scheduling priority of a process.
 *
 */
typedef uint8_t priority_t;

#define PRIORITY_MAX 63      ///< The maximum priority value, inclusive.
#define PRIORITY_MAX_USER 31 ///< The maximum priority user space is allowed to specify, inclusive.
#define PRIORITY_MIN 0       ///< The minimum priority value.

/**
 * @brief Spawn behaviour flags.
 * @enum spawn_flags_t
 */
typedef enum
{
    SPAWN_DEFAULT = 0, ///< Default spawn behaviour.
    /**
     * Starts the spawned process in a suspended state. The process will not begin executing until a "start" note is
     * received.
     *
     * The purpose of this flag is to allow the parent process to modify the child process before it starts executing,
     * for example modifying its environment variables.
     */
    SPAWN_SUSPEND = 1 << 0,
    SPAWN_EMPTY_FDS = 1 << 1,   ///< Dont inherit the file descriptors of the parent process.
    SPAWN_STDIO_FDS = 1 << 2,   ///< Only inherit stdin, stdout and stderr from the parent process.
    SPAWN_EMPTY_ENV = 1 << 3,   ///< Don't inherit the parent's environment variables.
    SPAWN_EMPTY_CWD = 1 << 4,   ///< Don't inherit the parent's current working directory, starts at root (/).
    SPAWN_EMPTY_GROUP = 1 << 5, ///< Don't inherit the parent's process group, instead create a new group.
    SPAWN_COPY_NS = 1 << 6,     ///< Don't share the parent's namespace, instead create a new copy of it.
    SPAWN_EMPTY_NS =
        1 << 7, ///< Create a new empty namespace, the new namespace will not contain any mountpoints or even a root.
    SPAWN_EMPTY_ALL = SPAWN_EMPTY_FDS | SPAWN_EMPTY_ENV | SPAWN_EMPTY_CWD | SPAWN_EMPTY_GROUP |
        SPAWN_EMPTY_NS ///< Empty all inheritable resources.
} spawn_flags_t;

/**
 * @brief System call for spawning new processes.
 *
 * By default, the spawned process will inherit the file table, environment variables, priority and current
 * working directory of the parent process by creating a copy. Additionally the child will exist within the same
 * namespace as the parent.
 *
 * @param argv A NULL-terminated array of strings, where `argv[0]` is the filepath to the desired executable.
 * @param flags Spawn behaviour flags.
 * @param pid Optional ouput pointer for the childs pid.
 * @return 
 */
static inline status_t spawn(const char** argv, spawn_flags_t flags, pid_t* pid)
{
    return syscall2(SYS_SPAWN, pid, (uint64_t)argv, flags);
}

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
 * to allocate virtual memory from userspace. An example usage would be to map the `/dev/const/zero` file which would
 * allocate zeroed memory.
 *
 * @param fd The open file descriptor of the file to be mapped.
 * @param addr The output pointer to store the virtual address, the value it currently points to is used as the desired virtual address. If it points to `NULL`, the kernel chooses an address.
 * address, will be rounded down to the nearest page multiple.
 * @param length The length of the segment to be mapped, note that this length will be rounded up to the nearest page
 * multiple by the kernel factoring in page boundaries.
 * @param prot Protection flags, must have at least `PROT_READ` set.
 * @return An appropriate status value.
 */
static inline status_t mmap(fd_t fd, void** addr, size_t length, prot_t prot)
{
    return syscall4(SYS_MMAP, (void*)addr, fd, (uint64_t)*addr, length, prot);
}

/**
 * @brief System call to unmap mapped memory.
 *
 * The `munmap()` function unmaps memory from the currently running processes address space.
 *
 * @param address The starting virtual address of the memory area to be unmapped.
 * @param length The length of the memory area to be unmapped.
 * @return An appropriate status value.
 */
static inline status_t munmap(void* address, size_t length)
{
    return syscall2(SYS_MUNMAP, NULL, (uintptr_t)address, length);
}

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
 * @return An appropriate status value.
 */
static inline status_t mprotect(void* address, size_t length, prot_t prot)
{
    return syscall3(SYS_MPROTECT, NULL, (uintptr_t)address, length, prot);
}

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
     * Wakes up a maximum of `val` number of threads that are currently waiting on the futex at the specified address.
     * If `val` is `FUTEX_ALL`, all waiting threads are woken up.
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
 * @param result Optional output pointer for the result.
 * @return An appropriate status value.
 */
static inline status_t futex(uint64_t* result, atomic_uint64_t* addr, uint64_t val, futex_op_t op, clock_t timeout)
{
    return syscall4(SYS_FUTEX, result, (uintptr_t)addr, val, op, timeout);
}

/**
 * @brief System call for retreving the time since boot.
 *
 * The `uptime()` function retrieves the system uptime since boot in clock ticks.
 *
 * @return The system uptime in clock ticks.
 */
static inline clock_t uptime(void)
{
    clock_t result;
    syscall0(SYS_UPTIME, &result);
    return result;
}

/**
 * @brief System call for sleeping.
 *
 * The `nanosleep()` function suspends the execution of the calling thread for a specified duration.
 *
 * @param timeout The duration in nanoseconds for which to sleep, if equal to `CLOCKS_NEVER` then it will sleep forever,
 * not sure why you would want to do that but you can.
 * @return On success, `0`. On failure, `_FAIL` and errno is set.
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

/**
 * @brief Note handler function type.
 */
typedef void (*note_func_t)(char* note);

/**
 * @brief System call that sets the handler to be called when a note is received.
 *
 * A note handler must either exit the thread or call `noted()`.
 *
 * If no handler is registered, the thread is killed when a note is received.
 *
 * @warning It is preferred to use `atnotify()` instead of this function as using this will prevent the standard library
 * from handling notes.
 *
 * @see kernel_ipc_note
 *
 * @param handler The handler function to be called on notes, can be `NULL` to unregister the current handler.
 * @return On success, `0`. On failure, `_FAIL` and errno is set.
 */
uint64_t notify(note_func_t handler);

/**
 * @brief System call that notifies the kernel that the current note has been handled.
 *
 * Should only be called from within a handler registered with `notify()` but NOT with `atnotify()`.
 *
 * If a note is not currently being handled, the thread is killed.
 *
 * @see kernel_ipc_note
 *
 * @return Never returns, instead resumes execution of the thread where it left off before the note was delivered.
 */
_NORETURN void noted(void);

/**
 * @brief Helper for comparing the first word of a string.
 *
 * @param string The string.
 * @param word The word to compare against.
 * @return On match, returns `0`. On mismatch, returns a non-zero value.
 */
int64_t wordcmp(const char* string, const char* word);

/**
 * @brief Action type for atnotify().
 * @enum atnotify_t
 */
typedef enum
{
    ATNOTIFY_ADD = 0,
    ATNOTIFY_REMOVE = 1
} atnotify_t;

/**
 * @brief User space `atnotify()` handler function type.
 */
typedef uint64_t (*atnotify_func_t)(char* note);

/**
 * @brief Adds or removes a handler to be called in user space when a note is received.
 *
 * If the return value of a handler is `_FAIL`, the process will exit.
 *
 * @param handler The handler function to be modified.
 * @param action The action to perform.
 * @return On success, `0`. On failure, `_FAIL` and errno is set.
 */
uint64_t atnotify(atnotify_func_t handler, atnotify_t action);

/**
 * @brief System call that handles pending notes for the current thread.
 *
 * Should only be called from an interrupt context.
 *
 * If the frame is not from user space, this function will return immediately.
 *
 * @param frame The interrupt frame of the current interrupt.
 * @return On success, `true` if a note was handled, `false` otherwise.
 */
_NORETURN void exits(const char* status);

/**
 * @brief Helper for sending the "kill" command to a process.
 *
 * @param pid The PID of the process to send the command to.
 * @return On success, `0`. On failure, `_FAIL` and `errno` is set.
 */
uint64_t kill(pid_t pid);

/**
 * @brief Architecture specific thread data codes.
 * @typedef arch_prctl_t
 */
typedef enum
{
    ARCH_GET_FS = 0, ///< Get the FS base address.
    ARCH_SET_FS = 1, ///< Set the FS base address.
} arch_prctl_t;

/**
 * @brief System call for setting architecture specific thread data.
 *
 * @param op The operation to perform.
 * @param addr If getting data, a pointer to store the retrieved address. If setting data, the address to set.
 * @return On success, `0`. On failure, `_FAIL` and `errno` is set.
 */
uint64_t arch_prctl(arch_prctl_t op, uintptr_t addr);

#if defined(__cplusplus)
}
#endif

#endif

/** @} */
