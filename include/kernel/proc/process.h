#pragma once

#include <kernel/drivers/perf.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/sysfs.h>
#include <kernel/fs/vfs_ctx.h>
#include <kernel/mem/space.h>
#include <kernel/proc/argv.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/futex.h>
#include <kernel/utils/ref.h>

#include <stdatomic.h>

/**
 * @brief Processes.
 * @defgroup kernel_proc Processes
 * @ingroup kernel
 *
 * Processes store the shared resources for threads of execution, for example the address space and open files.
 *
 * ## Process Filesystem
 *
 * Each process has a directory located at `/proc/[pid]`, which contains various files that can be used to interact with
 * the process. Additionally, there is a `/proc/self` bound mount point that points to the `/proc/[pid]` directory of
 * the current process. These files include:
 * - `prio`: A readable and writable file that contains the scheduling priority of the process.
 * - `cwd`: A readable file that contains the current working directory of the process.
 * - `cmdline`: A readable file that contains the command line arguments of the process (argv).
 * - `note`: A writable file that can be used to send notes (see `note_queue_t`) to the process.
 * - `wait`: A readable and pollable file that can be used to wait for the process to exit and retrieve its exit status.
 * - `perf`: A readable file that contains performance statistics for the process.
 *
 * ## Perf File Format
 *
 * The performance data in the `perf` file is presented in the following format:
 * ```
 * user_clocks kernel_clocks start_clocks user_pages thread_count
 * %lu %lu %lu %lu %lu
 * ```
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
    _Atomic(uint64_t) status;
    argv_t argv;
    space_t space;
    vfs_ctx_t vfsCtx;
    namespace_t namespace;
    futex_ctx_t futexCtx;
    perf_process_ctx_t perf;
    wait_queue_t dyingWaitQueue;
    atomic_bool isDying;
    process_threads_t threads;
    rwlock_t childrenLock;
    list_entry_t siblingEntry;
    list_t children;
    list_entry_t zombieEntry;
    struct process* parent;
    dentry_t* dir;         ///< The `/proc/[pid]` directory for this process.
    dentry_t* prioFile;    ///< The `/proc/[pid]/prio` file.
    dentry_t* cwdFile;     ///< The `/proc/[pid]/cwd` file.
    dentry_t* cmdlineFile; ///< The `/proc/[pid]/cmdline` file.
    dentry_t* noteFile;    ///< The `/proc/[pid]/note` file.
    dentry_t* waitFile;    ///< The `/proc/[pid]/wait` file.
    dentry_t* perfFile;    ///< The `/proc/[pid]/perf` file.
    mount_t* self;         ///< The `/proc/[pid]/self` mount point.
} process_t;

/**
 * @brief Allocates and initializes a new process.
 *
 * There is no `process_free()`, instead use `DEREF()`, `DEREF_DEFER()` or `process_kill()` to free a process.
 *
 * @param parent The parent process, can be `NULL`.
 * @param argv The argument vector, must be `NULL` terminated.
 * @param cwd The current working directory, can be `NULL` to inherit from the parent.
 * @param priority The priority of the new process.
 * @return On success, the newly created process. On failure, `NULL` and `errno` is set.
 */
process_t* process_new(process_t* parent, const char** argv, const path_t* cwd, priority_t priority);

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
void process_kill(process_t* process, uint64_t status);

/**
 * @brief Checks if a process is a child of another process.
 *
 * @param process The process to check.
 * @param parentId The parent process id.
 * @return `true` if the process is a child of the parent with id `parentId`, `false` otherwise.
 */
bool process_is_child(process_t* process, pid_t parentId);

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
