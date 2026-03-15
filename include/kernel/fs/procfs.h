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
 * The root of the filesystem contains a `clone` file. Opening the `clone` file will create a new process and return a
 * file descriptor to the root of that process's proc directory.
 *
 * @note Anytime a file descriptor is referred to it is from the perspective of the target process unless stated
 * otherwise.
 *
 * Included below is a list of contents for each processes proc directory.
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
 * ### give <parentfd> <childfd>
 *
 * Copies the `parentfd` from the caller process into the `childfd` of the target process.
 *
 * ### start <address>
 *
 * Creates the first thread of the process and starts execution at the specified virtual address.
 *
 * If the process already has threads, this command will fail.
 *
 * ### kill [result]
 *
 * Sends a kill note to all threads in the process, effectively terminating it. The optional result will be set as the
 * processes exit result.
 *
 * ## mem
 *
 * A writeable, readable and mappable file representing the address space of the process.
 *
 * The offset being accessed corresponds to the virtual address in the process's address space.
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
