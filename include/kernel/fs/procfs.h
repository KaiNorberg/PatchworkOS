#pragma once

#include <kernel/drivers/perf.h>
#include <kernel/fs/cwd.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/sysfs.h>
#include <kernel/ipc/note.h>
#include <kernel/mem/space.h>
#include <kernel/proc/group.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/futex.h>
#include <kernel/utils/ref.h>

#include <stdatomic.h>

/**
 * @brief Process filesystem.
 * @defgroup kernel_fs_procfs Process Filesystem
 * @ingroup kernel_fs
 *
 * Each process has a directory located at `/proc/[pid]/`, which contains various files that can be used to interact
 * with the process.
 *
 * Additionally, there is a `/proc/self` dynamic symlink that points to the `/proc/[pid]` directory of the current
 * process.
 *
 * Included below is a list of all entries found in the `/proc/[pid]/` directory.
 *
 * ## prio
 *
 * A readable and writable file that contains the scheduling priority of the process.
 *
 * Format:
 *
 * ```
 * %llu
 * ```
 *
 * ## cwd
 *
 * A readable and writable file that contains the current working directory of the process.
 *
 * Format:
 *
 * ```
 * %s
 * ```
 *
 * ## cmdline
 *
 * A readable file that contains the command line arguments of the process (argv).
 *
 * Format:
 *
 * ```
 * %s\0%s\0...%s\0
 * ```
 *
 * ## note
 *
 * A writable file that sends notes to the process. Writing to this file will enqueue that data as a
 * note in the note queue of one of the process's threads.
 *
 * @see kernel_ipc_note
 *
 * ## notegroup
 *
 * A writeable file that sends notes to every process in the group of the target process.
 *
 * @see kernel_ipc_note
 *
 * ## group
 *
 * Opening this file returns a file descriptor referring to the group. This file descriptor can be used with the
 * `setgroup` command in the `ctl` file to switch groups.
 *
 * ## pid
 *
 * A readable file that contains the process ID.
 *
 * Format:
 * ```
 * %llu
 * ```
 *
 * ## wait
 *
 * A readable and pollable file that can be used to wait for the process to exit. Reading
 * from this file will block until the process has exited.
 *
 * The read value is the exit status of the process, usually either a integer exit code or a string describing the
 * reason for termination often the note that caused it.
 *
 * Format:
 *
 * ```
 * %s
 * ```
 *
 * ## perf
 *
 * A readable file that contains performance statistics for the process.
 *
 * Format:
 *
 * ```
 * user_clocks kernel_sched_clocks start_clocks user_pages thread_count
 * %llu %llu %llu %llu %llu
 * ```
 *
 * ## ns
 *
 * Opening this file returns a file descriptor referring to the namespace. This file descriptor can be used with the
 * `setns` command in the `ctl` file to switch namespaces.
 *
 * ## ctl
 *
 * A writable file that can be used to control certain aspects of the process, such as closing file descriptors.
 *
 * Included is a list of all supported commands.
 *
 * @note Anytime a command refers to a file descriptor the file descriptor is the file descriptor of the target process,
 * not the current process.
 *
 * ### close <fd>
 *
 * Closes the specified file descriptor in the process.
 *
 * ### close <minfd> <maxfd>
 *
 * Closes the range `[minfd, maxfd)` of file descriptors in the process.
 *
 * Note that specifying `-1` as `maxfd` will close all file descriptors from `minfd` to the maximum allowed file
 * descriptor.
 *
 * ### dup2 <oldfd> <newfd>
 *
 * Duplicates the specified old file descriptor to the new file descriptor in the process.
 *
 * ### bind <target> <source>
 *
 * Bind a source path from the writing process to a target path in the processes namespace.
 *
 * Path flags for controlling the bind behaviour should to be specified in the target path.
 *
 * @see kernel_fs_path for information on path flags.
 *
 * ### mount <mountpoint> <fs> [device]
 *
 * Mounts a filesystem at the specified mountpoint in the process's namespace, optionally with a device.
 *
 * @see kernel_fs_path for information on path flags.
 *
 * ### touch <path>
 *
 * Open the specified path in the process and immediately close it.
 *
 * ### start
 *
 * Starts the process if it was previously suspended.
 *
 * ### kill
 *
 * Sends a kill note to all threads in the process, effectively terminating it.
 *
 * ### setns <fd>
 *
 * Sets the namespace of the process to the one referred to by the file descriptor.
 *
 * The file descriptor must be one that was opened from `/proc/[pid]/ns`.
 *
 * ### setgroup <fd>
 *
 * Sets the group of the process to the one referred to by the file descriptor.
 *
 * The file descriptor must be one that was opened from `/proc/[pid]/group`.
 *
 * ## fd
 *
 * @todo Implement the `/proc/[pid]/fd` directory.
 *
 * ## env
 *
 * A directory that contains the environment variables of the process. Each environment variable is represented as a
 * readable and writable file whose name is the name of the variable and whose content is the value of the variable.
 *
 * To add or modify an environment variable, create or write to a file with the name of the variable. To remove an
 * environment variable, delete the corresponding file.
 *
 * @{
 */

/**
 * @brief Process filesystem name.
 */
#define PROCFS_NAME "procfs"

/**
 * @brief Procfs inode numbers.
 * @enum procfs_inode_nums_t
 */
typedef enum
{
    PROCFS_INODE_ROOT = 0,
    PROCFS_INODE_SELF = 1,
    PROCFS_INODE_PRIO = 2,
    PROCFS_INODE_CWD = 3,
    PROCFS_INODE_CMDLINE = 4,
    PROCFS_INODE_NOTE = 5,
    PROCFS_INODE_NOTEGROUP = 6,
    PROCFS_INODE_GROUP = 7,
    PROCFS_INODE_PID = 8,
    PROCFS_INODE_WAIT = 9,
    PROCFS_INODE_PERF = 10,
    PROCFS_INODE_NS = 11,
    PROCFS_INODE_CTL = 12,
    PROCFS_INODE_FD = 13,
    PROCFS_INODE_ENV = 14,
    PROCFS_INODE_ENV_BASE = 15,
    PROCFS_INODE_PID_BASE = PROCFS_INODE_ENV_BASE + CONFIG_MAX_ENV_VARS,
} procfs_inode_nums_t;

/**
 * @brief A helper to get the inode number of a environment variable.
 * 
 * @param pid The process ID.
 * @param pos The position of the environment variable within its `/proc/[pid]/env/` directory.
 * @return The inode number of the environment variable.
 */
#define PROCFS_ENV_VAR_NUM(pid, pos) ((PROCFS_INODE_PID_BASE * (pid)) + PROCFS_INODE_ENV_BASE + ((pos) - DENTRY_DOTS_AMOUNT))

/**
 * @brief A helper to get the inode number of a process directory.
 * 
 * @param pid The process ID.
 * @return The inode number of the process directory.
 */
#define PROCFS_PID_NUM(pid) (PROCFS_INODE_PID_BASE * (pid))

/**
 * @brief A helper to get the inode number of a entry in a processes directory.
 * 
 * @param pid The process ID.
 * @param offset A value from the `procfs_inode_nums_t` enum to uniquely identify the file.
 * @return The inode number of the process directory entry.
 */
#define PROCFS_PID_ENTRY_NUM(pid, offset) (PROCFS_PID_NUM(pid) + (offset))

/**
 * @brief Register the procfs filesystem.
 */
void procfs_init(void);

/** @} */
