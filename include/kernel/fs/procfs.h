#pragma once

#include <kernel/drivers/perf.h>
#include <kernel/fs/cwd.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/namespace.h>
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
 * The "procfs" filesystem is used to expose process information and control interfaces to user space.
 *
 * Each process has its own directory whose name is the process ID and for convenience,
 * `/self` is a dynamic symbolic link to the current process's directory.
 *
 * Unlike traditional UNIX systems, security is implemented such that a process can access all files of all processes
 * that it could propagate mounts to, meaning any processes in its namespace or in child namespaces. If a process cant
 * propagate mounts to a another process then certain entries will appear to not exist. This is implemented using the
 * "revalidate()" dentry operation.
 *
 * @see kernel_fs_namespace
 *
 * Included below is a list of all entries found in each processes directory. All entries with restricted visibility
 * will be marked with `(restricted)`.
 *
 * @note Anytime a file descriptor is referred to it is from the perspective of the target process unless otherwise
 * stated.
 *
 * ## prio (restricted)
 *
 * A readable and writable file that contains the scheduling priority of the process.
 *
 * Format:
 *
 * ```
 * %llu
 * ```
 *
 * ## cwd (restricted)
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
 * ## note (restricted)
 *
 * A writable file that sends notes to the process. Writing to this file will enqueue that data as a
 * note in the note queue of one of the process's threads.
 *
 * @see kernel_ipc_note
 *
 * ## notegroup (restricted)
 *
 * A writeable file that sends notes to every process in the group of the target process.
 *
 * @see kernel_ipc_note
 *
 * ## group (restricted)
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
 * ## ns (restricted)
 *
 * Opening this file returns a file descriptor referring to the namespace. This file descriptor can be used with the
 * `setns` command in the `ctl` file to switch namespaces.
 *
 * ## ctl (restricted)
 *
 * A writable file that can be used to control certain aspects of the process, such as closing file descriptors.
 *
 * Included is a list of all supported commands.
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
 * ### kill [status]
 *
 * Sends a kill note to all threads in the process, effectively terminating it. The optional status will be set as the
 * processes exit status.
 *
 * ### setns <fd>
 *
 * Sets the namespace of the process to the one referred to by the file descriptor.
 *
 * The file descriptor must be one that was opened from `/[pid]/ns`.
 *
 * ### setgroup <fd>
 *
 * Sets the group of the process to the one referred to by the file descriptor.
 *
 * The file descriptor must be one that was opened from `/[pid]/group`.
 *
 * ## env (restricted)
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
 * @brief Register the procfs filesystem.
 */
void procfs_init(void);

/** @} */
