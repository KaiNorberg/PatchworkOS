#pragma once

#include <kernel/drivers/perf.h>
#include <kernel/fs/cwd.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/sysfs.h>
#include <kernel/mem/space.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/futex.h>
#include <kernel/utils/ref.h>

#include <stdatomic.h>

/**
 * @brief Processes.
 * @defgroup kernel_sched_processes Processes
 * @ingroup kernel_sched
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
 * A readable file that contains the current working directory of the process. 
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
 * @todo Reimplement cmdline.
 * 
 * ## note
 * 
 * A writable file that can be used to send notes to the process. Writing data to this file will enqueue that data as a note in the note queue of one of the process's threads.
 * 
 * ## wait
 * 
 * A readable and pollable file that can be used to wait for the process to exit and retrieve its exit status. Reading from this file will block until the process has exited.
 * 
 * Format:
 * 
 * ```
 * %lld
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
 * ## env
 * 
 * A directory that contains the environment variables of the process. Each environment variable is represented as a readable and writable file whose name is the name of the variable and whose content is the value of the variable.
 * 
 * To add or modify an environment variable, create or write to a file with the name of the variable. To remove an environment variable, delete the corresponding file.
 *
 * @{
 */

/**
 * @brief Process threads structure.
 *
 * Keeps track of all the threads in a process.
 */
typedef struct
{
    tid_t newTid;
    list_t list;
    lock_t lock;
} process_threads_t;

/**
 * @brief Process structure.
 * @struct process_t
 */
typedef struct process
{
    ref_t ref;
    pid_t id;
    _Atomic(priority_t) priority;
    _Atomic(int64_t) status;
    space_t space;
    namespace_t ns;
    cwd_t cwd;
    file_table_t fileTable;
    futex_ctx_t futexCtx;
    perf_process_ctx_t perf;
    wait_queue_t dyingWaitQueue;
    atomic_bool isDying;
    process_threads_t threads;
    list_entry_t zombieEntry;
    dentry_t* proc; ///< The `/proc/[pid]` directory, also stored in `dentries` for convenience.
    dentry_t* env; ///< The `/proc/[pid]/env` directory, also stored in `dentries` for convenience.
    list_t dentries; ///< List of dentries in the `/proc/[pid]/` directory.
    list_t envVars;  ///< List of dentries in the `/proc/[pid]/env/` directory.
    lock_t dentriesLock;
    mount_t* self;         ///< The `/proc/[pid]/self` mount point.
} process_t;

/**
 * @brief Allocates and initializes a new process.
 *
 * There is no `process_free()`, instead use `DEREF()`, `DEREF_DEFER()` or `process_kill()` to free a process.
 *
 * @param priority The priority of the new process.
 * @return On success, the newly created process. On failure, `NULL` and `errno` is set.
 */
process_t* process_new(priority_t priority);

/**
 * @brief Kills a process.
 *
 * Sends a kill note to all threads in the process and sets its exit status.
 * Will also close all files opened by the process and deinitialize its `/proc` directory.
 *
 * When all threads have exited and all entires in its `/proc` directory have been closed, the process will be freed.
 *
 * @param process The process to kill.
 * @param status The exit status of the process.
 */
void process_kill(process_t* process, int32_t status);

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

/**
 * @brief Initializes the process reaper.
 *
 * The process reaper allows us to delay the freeing of processes, this is useful if, for example, another process
 * wanted that process's exit status.
 */
void process_reaper_init(void);

/** @} */
