#pragma once

#include "argv.h"
#include "fs/sysfs.h"
#include "fs/vfs_ctx.h"
#include "mem/space.h"
#include "sched/sched.h"
#include "sched/wait.h"
#include "sync/futex.h"
#include <stdatomic.h>

/**
 * @brief Processes.
 * @defgroup kernel_proc Processes
 * @ingroup kernel
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
    list_t aliveThreads;
    list_t zombieThreads;
    lock_t lock;
} process_threads_t;

/**
 * @brief Process directory structure.
 *
 * Holds all the sysfs entries for a process.
 */
typedef struct
{
    sysfs_dir_t dir;
    sysfs_file_t prioFile;
    sysfs_file_t cwdFile;
    sysfs_file_t cmdlineFile;
    sysfs_file_t noteFile;
    sysfs_file_t statusFile;
} process_dir_t;

/**
 * @brief Process structure.
 * @struct process_t
 */
typedef struct process
{
    pid_t id;
    _Atomic(priority_t) priority;
    _Atomic(uint64_t) status;
    argv_t argv;
    vfs_ctx_t vfsCtx;
    space_t space;
    futex_ctx_t futexCtx;
    wait_queue_t dyingWaitQueue;
    atomic_bool isDying;
    process_threads_t threads;
    list_entry_t entry;
    list_t children;
    struct process* parent;
    process_dir_t dir;
} process_t;

/**
 * @brief Allocates and initializes a new process.
 *
 * @param parent The parent process, can be `NULL`.
 * @param argv The argument vector, must be `NULL` terminated.
 * @param cwd The current working directory, can be `NULL` to inherit from the parent.
 * @param priority The priority of the new process.
 * @return On success, the newly created process. On failure, `NULL` and `errno` is set.
 */
process_t* process_new(process_t* parent, const char** argv, const path_t* cwd, priority_t priority);

/**
 * @brief Deinitializes and frees a process structure.
 *
 * @param process The process to free.
 */
void process_free(process_t* process);

/**
 * @brief Kills a process.
 *
 * Sends a kill note to all threads in the process and sets its exit status.
 * Also closes all files in the processes vfs context to notify blocking threads.
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
 * @return Tthe kernel process.
 */
process_t* process_get_kernel(void);

/**
 * @brief Initializes the `/proc` directory.
 */
void process_procfs_init(void);

/** @} */
