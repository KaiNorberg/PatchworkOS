#ifndef _SYS_PROC_H
#define _SYS_PROC_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include <libc/fs.h>
#include <libc/syscall.h>

#include "_libc/NULL.h"
#include "_libc/PAGE_SIZE.h"
#include "_libc/clock_t.h"
#include "_libc/config.h"

/**
 * @brief Process management.
 * @ingroup libc
 * @defgroup libc_proc Processes
 *
 * @{
 */

extern char** environ; ///< Pointer to the environment variables.

/**
 * @brief Process Identifier.
 * @ingroup libc
 *
 * The `proc_t` type is used to store process identifiers, valid id's will map to folders found in `/proc`.
 *
 */
typedef __UINT64_TYPE__ proc_t;

/**
 * @brief Scheduler priority type.
 * @typedef prio_t
 *
 */
typedef uint8_t prio_t;

#define PRIO_MAX 63      ///< The maximum priority value, inclusive.
#define PRIO_MAX_USER 31 ///< The maximum priority user space is allowed to specify, inclusive.
#define PRIO_DEFAULT 15  ///< The default priority value.
#define PRIO_MIN 0       ///< The minimum priority value.

/**
 * @brief Process creation behaviour flags.
 */
typedef uint64_t proc_flags_t;
#define PROC_DEFAULT 0 ///< Default behaviour, the child process inherits no resources from the parent process.
/**
 * Starts the created process in a suspended state. The process will not begin executing until a "start" note is
 * received.
 *
 * The purpose of this flag is to allow the parent process to modify the child process before it starts executing,
 * primarily via the "procfs" filesystem.
 *
 * @see kernel_fs_procfs
 */
#define PROC_SUSPEND (1 << 0)
/**
 * Creates the process within the same job as the parent process. If not specified, the process
 * will be created in a new job.
 */
#define PROC_INHERIT_JOB (1 << 1)

/**
 * @brief File descriptor mapping for process creation.
 * @struct proc_fd_t
 */
typedef struct proc_fd
{
    fd_t parent; ///< The file descriptor in the parent process.
    fd_t child;  ///< The desired file descriptor index in the child process.
} proc_fd_t;

/**
 * @brief Process arguments structure.
 * @struct proc_args_t
 *
 * @note We choose to use a single buffer of a specified length to reduce the risk for vulnerabilities when copying the
 * arguments in the kernel.
 */
typedef struct proc_args
{
    const char* buf; ///< A null deliminated string of arguments.
    size_t len;
} proc_args_t;

/**
 * @brief Process environment variables structure.
 * @struct proc_envp_t
 */
typedef proc_args_t proc_envp_t;
 
/**
 * @brief Helper macro for creating a process arguments buffer.
 *
 * @param ... A list of strings to be used as arguments.
 */
#define PROC_ARGS(...) \
    ({ \
        const char* _argv[] = {__VA_ARGS__}; \
        size_t _argc = sizeof(_argv) / sizeof(_argv[0]); \
        size_t _len = 0; \
        for (size_t _i = 0; _i < _argc; _i++) \
        { \
            _len += strlen(_argv[_i]) + 1; \
        } \
        char* _buf = alloca(_len); \
        char* _ptr = _buf; \
        for (size_t _i = 0; _i < _argc; _i++) \
        { \
            size_t _l = strlen(_argv[_i]); \
            memcpy(_ptr, _argv[_i], _l); \
            _ptr[_l] = '\0'; \
            _ptr += _l + 1; \
        } \
        (proc_args_t){.buf = _buf, .len = _len}; \
    })

/**
 * @brief Helper macro for creating a process environment buffer.
 *
 * @param ... A list of strings to be used as environment variables.
 */
#define PROC_ENVP(...) PROC_ARGS(...)

/**
 * @brief Sentinel value to indicate that the child process should inherit the parent's environment.
 */
#define PROC_ENVP_INHERIT (proc_envp_t){.buf = NULL, .len = 0}

/**
 * @brief System call for creating new processes.
 *
 * @param cwd The current working directory to for resolving paths.
 * @param root The root directory to use for resolving paths.
 * @param args The arguments for the new process.
 * @param envp The environment variables for the new process.
 * @param fds An array of file descriptor mappings.
 * @param count The number of mappings in the array.
 * @param priority The priority of the new process.
 * @param flags Creation behaviour flags.
 * @param proc Optional output pointer for the proc directory of the child.
 * @return An appropriate status value.
 */
status_t proc_create(fd_t cwd, fd_t root, proc_args_t args, proc_envp_t envp, const proc_fd_t* fds, size_t count, prio_t priority,
    proc_flags_t flags, fd_t* proc);

/**
 * @brief System call to retrieve the current pid.
 *
 * @return The running processes pid.
 */
static inline proc_t proc_current(void)
{
    proc_t pid;
    syscall0(SYS_PROC_CURRENT, &pid);
    return pid;
}

/**
 * @brief Helper for sending the "kill" note to a process.
 *
 * @param pid The PID of the process to send the note to.
 * @return An appropriate status value.
 */
status_t proc_kill(proc_t pid);

/**
 * @brief System call that exists the current process.
 *
 * @param result The string exit result of the process.
 */
_NORETURN void proc_exit(const char* result);

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
 * @brief Number of bits in a byte.
 */
#define BITS_IN_BYTE 8

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
