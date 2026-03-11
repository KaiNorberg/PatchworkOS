#pragma once

#include <kernel/drivers/perf.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file_table.h>
#include <kernel/ipc/note.h>
#include <kernel/mem/space.h>
#include <kernel/proc/job.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/sync.h>
#include <kernel/utils/ref.h>

#include <stdatomic.h>

/**
 * @brief Process filesystem.
 * @defgroup kernel_fs_procfs Process Filesystem
 * @ingroup kernel_fs
 *
 * The "procfs" filesystem is used to expose process information and control interfaces to user space.
 *
 * Each process has its own directory whose name is its process ID and for convenience,
 * `/self` is a dynamic symbolic link to the current process's directory.
 *
 * Unlike traditional UNIX systems, a process can only see the proc directories of processes within its own job or
 * child jobs. It is however possible for a process to pass a file descriptor to its own proc directory to another
 * process, allowing it to be controlled by that process.
 *
 * @note Anytime a file descriptor is referred to it is from the perspective of the target process unless stated
 * otherwise.
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
 * If the target process is the leader of its job, the note will be sent to all processes in the job.
 *
 * @see kernel_ipc_note
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
 * ## ctl
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
 * ### dup <oldfd> <newfd>
 *
 * Duplicates the specified old file descriptor to the new file descriptor in the process.
 *
 * ### start
 *
 * Starts the process if it was previously suspended.
 *
 * ### kill [result]
 *
 * Sends a kill note to all threads in the process, effectively terminating it. The optional result will be set as the
 * processes exit result.
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
