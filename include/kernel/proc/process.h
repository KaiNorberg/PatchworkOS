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
 * @brief Processes.
 * @defgroup kernel_proc_process Processes
 * @ingroup kernel_proc
 *
 * Processes store the shared resources for threads of execution, for example the address space and open files.
 *
 * ## Process Filesystem
 *
 * Each process has a directory located at `/proc/[pid]`, which contains various files that can be used to interact with
 * the process. Additionally, there is a `/proc/self` bound mount point that points to the `/proc/[pid]` directory of
 * the current process.
 *
 * Included below is a list of all entries found in the `/proc/[pid]` directory along with their formats.
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
 * ## gid
 *
 * A readable file that contains the group ID of the process.
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
 * user_clocks kernel_clocks start_clocks user_pages thread_count
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
 * ### dup2 <oldfd> <newfd>
 *
 * Duplicates the specified old file descriptor to the new file descriptor in the process.
 *
 * ### start
 *
 * Starts the process if it was previously suspended.
 *
 * ### kill
 *
 * Sends a kill note to all threads in the process, effectively terminating it.
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
 * @brief Represents the threads in a process.
 * @struct process_threads_t
 */
typedef struct
{
    tid_t newTid;
    list_t list;
    lock_t lock;
} process_threads_t;

/**
 * @brief Process flags enum.
 * @enum process_flags_t
 */
typedef enum
{
    PROCESS_NONE = 0,
    PROCESS_DYING = 1 << 0,
    PROCESS_SUSPENDED = 1 << 1,
} process_flags_t;

/**
 * @brief Process exit status structure.
 * @struct process_exit_status_t
 */
typedef struct
{
    char buffer[NOTE_MAX];
    lock_t lock;
} process_exit_status_t;

/**
 * @brief Process `/proc/[pid]` directory structure.
 * @struct process_dir_t
 */
typedef struct
{
    mount_t* self;   ///< The `/proc/self` bind mount.
    dentry_t* dir; ///< The `/proc` directory.
    list_t files; ///< List of file dentries for the `/proc/[pid]/` directory.
    dentry_t* env; ///< The `/proc/[pid]/env` directory.
    list_t envEntries; ///< List of environment variable dentries.
    lock_t lock;
} process_dir_t;

/**
 * @brief Process structure.
 * @struct process_t
 */
typedef struct process
{
    pid_t id;
    group_entry_t groupEntry;
    _Atomic(priority_t) priority;
    process_exit_status_t exitStatus;
    space_t space;
    namespace_t ns;
    cwd_t cwd;
    file_table_t fileTable;
    futex_ctx_t futexCtx;
    perf_process_ctx_t perf;
    note_handler_t noteHandler;
    wait_queue_t suspendQueue;
    wait_queue_t dyingQueue;
    _Atomic(process_flags_t) flags;
    process_threads_t threads;
    list_entry_t zombieEntry;
    process_dir_t dir;
    char* cmdline;
    uint64_t cmdlineSize;
} process_t;

/**
 * @brief Allocates and initializes a new process.
 *
 * There is no `process_free()`, instead use `UNREF()`, `UNREF_DEFER()` or `process_kill()` to free a process.
 *
 * @param priority The priority of the new process.
 * @param gid The group ID of the new process, or `GID_NONE` to create a new group.
 * @return On success, the newly created process. On failure, `NULL` and `errno` is set.
 */
process_t* process_new(priority_t priority, gid_t gid);

/**
 * @brief Kills a process, pushing it to the reaper.
 *
 * @param process The process to kill.
 * @param status The exit status of the process.
 */
void process_kill(process_t* process, const char* status);

/**
 * @brief Deinitializes the `/proc/[pid]` directory of a process.
 *
 * When there are no more references to any of the entries in the `/proc/[pid]` directory, the process will be freed.
 * 
 * @param process The process whose `/proc/[pid]` directory will be deinitialized.
 */
void process_dir_deinit(process_t* process);

/**
 * @brief Copies the environment variables from one process to another.
 *
 * @param dest The destination process, must have an empty environment.
 * @param src The source process.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EBUSY`: The destination process already has environment variables.
 */
uint64_t process_copy_env(process_t* dest, process_t* src);

/**
 * @brief Sets the command line arguments for a process.
 *
 * This value is only used for the `/proc/[pid]/cmdline` file.
 *
 * @param process The process to set the cmdline for.
 * @param argv The array of argument strings.
 * @param argc The number of arguments.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOMEM`: Out of memory.
 */
uint64_t process_set_cmdline(process_t* process, char** argv, uint64_t argc);

/**
 * @brief Checks if a process has a thread with the specified thread ID.
 *
 * @param process The process to check.
 * @param tid The thread ID to look for.
 * @return `true` if the process has a thread with the specified ID, `false` otherwise.
 */
bool process_has_thread(process_t* process, tid_t tid);

/**
 * @brief Gets the kernel process.
 *
 * The kernel process will be initalized lazily on the first call to this function, which should happen during early
 * boot.
 *
 * Will never return `NULL`.
 *
 * Will not increment the reference count of the returned process as it should never be freed either way.
 *
 * @return The kernel process.
 */
process_t* process_get_kernel(void);

/**
 * @brief Initializes the `/proc` directory.
 */
void process_procfs_init(void);

/** @} */
