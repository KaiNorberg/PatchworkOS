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

#include "_libstd/NULL.h"
#include "_libstd/PAGE_SIZE.h"
#include "_libstd/clock_t.h"
#include "_libstd/config.h"
#include "_libstd/fd_t.h"

/**
 * @brief Process management.
 * @ingroup libstd
 * @defgroup libstd_sys_proc Processes
 *
 * @{
 */

/**
 * @brief Process Identifier.
 * @ingroup libstd
 *
 * The `proc_t` type is used to store process identifiers, valid id's will map to folders found in `/proc`.
 *
 */
typedef __UINT64_TYPE__ proc_t;

/**
 * @brief Scheduler priority type.
 * @typedef proc_prio_t
 *
 */
typedef uint8_t proc_prio_t;

#define PROC_PRIO_MAX 63      ///< The maximum priority value, inclusive.
#define PROC_PRIO_MAX_USER 31 ///< The maximum priority user space is allowed to specify, inclusive.
#define PROC_PRIO_MIN 0       ///< The minimum priority value.

/**
 * @brief Process creation behaviour flags.
 * @enum proc_flags_t
 */
typedef enum
{
    PROC_DEFAULT = 0, ///< Default creation behaviour.
    /**
     * Starts the created process in a suspended state. The process will not begin executing until a "start" note is
     * received.
     *
     * The purpose of this flag is to allow the parent process to modify the child process before it starts executing,
     * for example modifying its environment variables.
     */
    PROC_SUSPEND = 1 << 0,
    PROC_EMPTY_FDS = 1 << 1,   ///< Dont inherit the file descriptors of the parent process.
    PROC_STDIO_FDS = 1 << 2,   ///< Only inherit stdin, stdout and stderr from the parent process.
    PROC_EMPTY_ENV = 1 << 3,   ///< Don't inherit the parent's environment variables.
    PROC_EMPTY_CWD = 1 << 4,   ///< Don't inherit the parent's current working directory, starts at root (/).
    PROC_EMPTY_GROUP = 1 << 5, ///< Don't inherit the parent's process group, instead create a new group.
    PROC_COPY_NS = 1 << 6,     ///< Don't share the parent's namespace, instead create a new copy of it.
    PROC_EMPTY_NS =
        1 << 7, ///< Create a new empty namespace, the new namespace will not contain any mountpoints or even a root.
    PROC_EMPTY_ALL = PROC_EMPTY_FDS | PROC_EMPTY_ENV | PROC_EMPTY_CWD | PROC_EMPTY_GROUP |
        PROC_EMPTY_NS ///< Empty all inheritable resources.
} proc_flags_t;

/**
 * @brief System call for creating new processes.
 *
 * By default, the created process will inherit the file table, environment variables, priority and current
 * working directory of the parent process by creating a copy. Additionally the child will exist within the same
 * namespace as the parent.
 *
 * @param argv A NULL-terminated array of strings, where `argv[0]` is the filepath to the desired executable.
 * @param flags Creation behaviour flags.
 * @param pid Optional ouput pointer for the childs pid.
 * @return
 */
static inline status_t proc_create(const char** argv, proc_flags_t flags, proc_t* pid)
{
    return syscall2(SYS_PROC_CREATE, pid, (uint64_t)argv, flags);
}

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
 * @brief System call that exists the current process.
 *
 * @param result The string exit result of the process.
 */
_NORETURN void proc_exit(const char* result);

/**
 * @brief Helper for sending the "kill" note to a process.
 *
 * @param pid The PID of the process to send the note to.
 * @return An appropriate status value.
 */
status_t proc_kill(proc_t pid);

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
