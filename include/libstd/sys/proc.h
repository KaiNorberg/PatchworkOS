#ifndef _SYS_PROC_H
#define _SYS_PROC_H 1

#include <stdint.h>
#include <sys/atomint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/ERR.h"
#include "_AUX/NULL.h"
#include "_AUX/clock_t.h"
#include "_AUX/config.h"
#include "_AUX/fd_t.h"
#include "_AUX/pid_t.h"
#include "_AUX/tid_t.h"

/**
 * @brief Process management header.
 * @ingroup libstd
 * @defgroup libstd_sys_proc Process management
 *
 * The `sys/proc.h` header handles process management, including process creation, managing a processes address space,
 * scheduling, and similar.
 *
 */

/**
 * @brief Stucture used to duplicate fds in `spawn()`.
 * @ingroup libstd_sys_proc
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
 * @ingroup libstd_sys_proc
 *
 * The `spawn_flags_t` enum is used to modify the behaviour when spawning a new process, for more information check the
 * `spawn()` function.
 *
 */
typedef enum
{
    SPAWN_NONE, //!< None
} spawn_flags_t;

/**
 * @brief Spawn fds termination constant.
 * @ingroup libstd_sys_proc
 *
 * The `SPAWN_FD_END` constant is used to terminate the `fds` array used in the `spawn()` function.
 *
 */
#define SPAWN_FD_END \
    (spawn_fd_t) \
    { \
        .child = FD_NONE, .parent = FD_NONE \
    }

/**
 * @brief System call for creating child processes.
 * @ingroup libstd_sys_proc
 *
 * The `spawn()` function creates and executes a new process.
 *
 * @param argv A NULL-terminated array of strings, where `argv[0]` is the filepath to the desired executable. This array
 * will be pushed to the child stack and the child can find a pointer to this array in its rsi register, along with its
 * length in the rdi register.
 * @param fds A array of file descriptors to be duplicated to the child process. Each `spawn_fd_t` in the array
 * specifies a source file descriptor in the parent (`.parent`) and its destination in the child (`.child`).
 * The array must be terminated by `SPAWN_FD_END`.
 * @param cwd The working directory for the child process. If `NULL`, the child inherits the parent's current
 * working directory.
 * @param flags Flags to control the spawning behavior, currently no spawn flags are implemented.
 * @return On success, returns the childs pid, on failure returns `ERR` and errno is set.
 */
pid_t spawn(const char** argv, const spawn_fd_t* fds, const char* cwd, spawn_flags_t flags);

/**
 * @brief System call to retrieve the current pid.
 * @ingroup libstd_sys_proc
 *
 * The `getpid()` function retrieves the pid of the currently running process.
 *
 * @return The running processes pid.
 */
pid_t getpid(void);

/**
 * @brief System call to retrieve the current tid.
 * @ingroup libstd_sys_proc
 *
 * The `gettid()` function retrieves the tid of the currently running thread.
 *
 * @return The running threads tid.
 */
tid_t gettid(void);

/**
 * @brief Memory page size
 * @ingroup libstd_sys_proc
 *
 * The `PAGE_SIZE` constant stores the size in bytes of one page in memory.
 *
 */
#define PAGE_SIZE 0x1000

/**
 * @brief Convert bytes to pages.
 * @ingroup libstd_sys_proc
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
 * @ingroup libstd_sys_proc
 *
 * The `PAGE_SIZE_OF()` macro returns the amount of pages in memory required to store a given object.
 *
 * @param object The object to calculate the page size of.
 * @return The amount of pages.
 */
#define PAGE_SIZE_OF(object) BYTES_TO_PAGES(sizeof(object))

/**
 * @brief Memory protection flags.
 * @ingroup libstd_sys_proc
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
 * @ingroup libstd_sys_proc
 *
 * The `mmap()` function maps memory to the currently running processes address space from a file, this is the only way
 * to allocate virtual memory from userspace. An example usage would be to map the `sys:/zero` file which would allocate
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
 * @ingroup libstd_sys_proc
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
 * @ingroup libstd_sys_proc
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
 * @ingroup libstd_sys_proc
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
 * @ingroup libstd_sys_proc
 *
 * The `FUTEX_ALL` constant can be used as the `val` argument when using the `FUTEX_WAIT` operating in the `futex()`
 * function to wake upp all waiters.
 *
 */
#define FUTEX_ALL UINT64_MAX

/**
 * @brief System call for fast user space mutual exclusion.
 * @ingroup libstd_sys_proc
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
uint64_t futex(atomic_uint64* addr, uint64_t val, futex_op_t op, clock_t timeout);

/**
 * @brief System call for retreving the time since boot.
 * @ingroup libstd_sys_proc
 *
 * The `uptime()` function retrieves the system uptime since boot in clock ticks.
 *
 * @return The system uptime in clock ticks.
 */
clock_t uptime(void);

/**
 * @brief System call for sleeping.
 * @ingroup libstd_sys_proc
 *
 * The `sleep()` function suspends the execution of the calling thread for a specified duration.
 *
 * @param timeout The duration in clock ticks for which to sleep, if equal to `CLOCKS_NEVER` then it will sleep forever,
 * not sure why you would want to do that but you can.
 * @return On success, returns 0. On failure, returns `ERR` and errno is set.
 */
uint64_t sleep(clock_t timeout);

#if defined(__cplusplus)
}
#endif

#endif
