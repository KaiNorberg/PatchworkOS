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
 */
typedef uint64_t proc_flags_t;
#define PROC_EMPTY 0 ///< Default behaviour, the child process inherits no resources from the parent process.
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
#define PROC_FDIN (1 << 1)  ///< Inherit the parent's standard input file descriptor.
#define PROC_FDOUT (1 << 2) ///< Inherit the parent's standard output file descriptor.
#define PROC_FDERR (1 << 3) ///< Inherit the parent's standard error file descriptor.
#define PROC_FDCWD (1 << 4) ///< Inherit the parent's current working directory file descriptor.
#define PROC_FDROOT (1 << 5)
#define PROC_IOALL \
    (PROC_FDIN | PROC_FDOUT | PROC_FDERR | PROC_FDCWD | PROC_FDROOT) ///< Inherit all standard file descriptors.
#define PROC_FD (1 << 6)                                             ///< Inherit the parent's file descriptors.
#define PROC_ENV (1 << 7)                                            ///< Inherit the parent's environment variables.
#define PROC_GROUP (1 << 8)                                          ///< Inherit the parent's process group.
#define PROC_PRIO (1 << 9)                                           ///< Inherit the parent's scheduling priority.
#define PROC_ALL (PROC_FD | PROC_ENV | PROC_GROUP | PROC_PRIO)       ///< Inherit all resources.

/**
 * @brief System call for creating new processes.
 *
 * @param argv A NULL-terminated array of strings, where `argv[0]` is the filepath to the desired executable.
 * @param flags Creation behaviour flags.
 * @param proc Optional ouput pointer for the childs identifier.
 * @return An appropriate status value.
 */
static inline status_t proc_create(const char** argv, proc_flags_t flags, proc_t* proc)
{
    return syscall2(SYS_PROC_CREATE, proc, (uint64_t)argv, flags);
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
