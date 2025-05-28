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

typedef struct
{
    fd_t child;
    fd_t parent;
} spawn_fd_t;

typedef enum
{
    SPAWN_NONE,
} spawn_flags_t;

#define SPAWN_FD_END \
    (spawn_fd_t) \
    { \
        .child = FD_NONE, .parent = FD_NONE \
    }

/**
 * @brief Spawn system call.
 *
 * The `spawn` function creates and executes a new process.
 *
 * @param argv A NULL-terminated array of strings, where `argv[0]` is the filepath to the desired executable. This array
 * will be pushed to the child stack and the child can find a pointer to this array in its rsi register, along with its
 * length in the rdi register.
 * @param fds A list of file descriptors to be duplicated to the child process. Each `spawn_fd_t` in the list
 * specifies a source file descriptor in the parent (`.parent`) and its destination in the child (`.child`).
 * The list must be terminated by `SPAWN_FD_END`.
 * @param cwd The working directory for the child process. If `NULL`, the child inherits the parent's current
 * working directory.
 * @param flags Flags to control the spawning behavior, currently no spawn flags are implemented.
 * @return On success, returns the childs pid, on failure returns `ERR` and errno is set.
 */
pid_t spawn(const char** argv, const spawn_fd_t* fds, const char* cwd, spawn_flags_t flags);

/**
 * @brief Getpid system call.
 *
 * The `getpid` function retrieves the pid of the currently running process.
 *
 * @return The running processes pid.
 */
pid_t getpid(void);

/**
 * @brief Gettid system call.
 *
 * The `gettid` function retrieves the tid of the currently running thread.
 *
 * @return The running threads tid.
 */
tid_t gettid(void);

#define PAGE_SIZE 0x1000
#define SIZE_IN_PAGES(size) (((size) + PAGE_SIZE - 1) / PAGE_SIZE)
#define PAGE_SIZE_OF(object) SIZE_IN_PAGES(sizeof(object))

typedef enum prot
{
    PROT_NONE = 0,
    PROT_READ = (1 << 0),
    PROT_WRITE = (1 << 1)
} prot_t;

/**
 * @brief Mmap system call.
 *
 * The `mmap` function maps memory to the currently running processes address space from a file, this is the only way to allocate virtual memory from userspace. An
 * example usage would be to map the `sys:/zero` file which would allocate zeroed memory.
 *
 * @param fd The open file descriptor of the file to be mapped.
 * @param address The desired virtual destination address, if equal to `NULL` the kernel will choose a available
 * address, will be rounded down to the nearest page multiple.
 * @param length The length of the segment to be mapped, note that this length will be rounded up to the nearest page
 * multiple by the kernel factoring in page boundaries.
 * @param prot Protection flags.
 * @return On success, returns the address of the mapped memory, will always be page aligned, on failure returns `ERR` and errno is set.
 */
void* mmap(fd_t fd, void* address, uint64_t length, prot_t prot);

/**
 * @brief Munmap system call.
 *
 * The `munmap` function unmaps memory from the currently running processes address space.
 * 
 * @param address The starting virtual address of the memory area to be unmapped.
 * @param length The length of the memory area to be unmapped.
 * @return On success, returns 0, on failure returns `ERR` and errno is set.
 */
uint64_t munmap(void* address, uint64_t length);

/**
 * @brief Mprotect system call.
 * 
 * The `mprotect` changes the protection flags of a virtual memory area in the currently running processes address space.
 * 
 * @param address  The starting virtual address of the memory area to be modified.
 * @param length The length of the memory area to be modifed.
 * @param prot The new protection flags of the memory area.
 * @return On success, returns 0, on failure returns `ERR` and errno is set.
 */
uint64_t mprotect(void* address, uint64_t length, prot_t prot);

typedef enum
{
    FUTEX_WAIT,
    FUTEX_WAKE,
} futex_op_t;

#define FUTEX_ALL UINT64_MAX

/**
 * @brief Futex system call.
 *
 * The `futex` function provides a fast user-space syncronization mechanism. It can be used to implement userspace mutexes, conditional variables, etc.
 *
 * @param addr A pointer to an atomic 64-bit unsigned integer.
 * @param val The value to compare against for `FUTEX_WAIT` or the number of threads to wake for `FUTEX_WAKE`.
 * @param op The futex operation to perform (e.g., `FUTEX_WAIT` or `FUTEX_WAKE`).
 * @param timeout An optional timeout for `FUTEX_WAIT`. If `CLOCKS_NEVER`, it waits forever.
 * @return On success, returns 0, except if using the `FUTEX_WAKE` operation then it returns the number of woken threads. On failure, returns `ERR` and errno is set.
 */
uint64_t futex(atomic_uint64* addr, uint64_t val, futex_op_t op, clock_t timeout);

/**
 * @brief Uptime system call.
 *
 * The `uptime` function retrieves the system uptime since boot in clock ticks.
 *
 * @return The system uptime in clock ticks.
 */
clock_t uptime(void);

/**
 * @brief Sleep system call.
 *
 * The `sleep` function suspends the execution of the calling thread for a specified duration.
 *
 * @param timeout The duration in clock ticks for which to sleep, if equal to `CLOCKS_NEVER` then it will sleep forever, not sure why you would want to do that but you can.
 * @return On success, returns 0. On failure, returns `ERR` and errno is set.
 */
uint64_t sleep(clock_t timeout);

#if defined(__cplusplus)
}
#endif

#endif
