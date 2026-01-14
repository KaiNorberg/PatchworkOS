#pragma once

#include <kernel/drivers/perf.h>
#include <kernel/fs/cwd.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/namespace.h>
#include <kernel/ipc/note.h>
#include <kernel/mem/space.h>
#include <kernel/proc/env.h>
#include <kernel/proc/group.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/futex.h>
#include <kernel/sync/rcu.h>
#include <kernel/utils/map.h>
#include <kernel/utils/ref.h>

#include <stdatomic.h>

/**
 * @brief Process management.
 * @defgroup kernel_proc Process
 * @ingroup kernel
 *
 * Processes store the shared resources for threads of execution, for example the address space and open files.
 *
 * @{
 */

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
 * @brief Represents the threads in a process.
 * @struct process_threads_t
 */
typedef struct
{
    _Atomic(tid_t) newTid;
    list_t list; ///< Reads are RCU protected, writes require the lock.
    uint64_t count;
    lock_t lock;
} process_threads_t;

/**
 * @brief Maximum length of a process exit status.
 */
#define PROCESS_STATUS_MAX 256

/**
 * @brief Process exit status structure.
 * @struct process_status_t
 */
typedef struct
{
    char buffer[PROCESS_STATUS_MAX];
    lock_t lock;
} process_status_t;

/**
 * @brief Process structure.
 * @struct process_t
 */
typedef struct process
{
    ref_t ref;
    list_entry_t entry;
    map_entry_t mapEntry;
    list_entry_t zombieEntry;
    pid_t id;
    _Atomic(priority_t) priority;
    process_status_t status;
    space_t space;
    namespace_t* nspace;
    lock_t nspaceLock;
    cwd_t cwd;
    file_table_t fileTable;
    futex_ctx_t futexCtx;
    perf_process_ctx_t perf;
    note_handler_t noteHandler;
    wait_queue_t suspendQueue;
    wait_queue_t dyingQueue;
    _Atomic(process_flags_t) flags;
    process_threads_t threads;
    env_t env;
    char** argv;
    uint64_t argc;
    group_member_t group;
    rcu_entry_t rcu;
} process_t;

/**
 * @brief Global list of all processes.
 *
 * @warning Should only be read while in a RCU read-side critical section.
 */
extern list_t _processes;

/**
 * @brief Allocates and initializes a new process.
 *
 * It is the responsibility of the caller to `UNREF()` the returned process.
 *
 * @param priority The priority of the new process.
 * @param group A member of the group to add the new process to, or `NULL` to create a new group for the process.
 * @param ns The namespace to use for the new process.
 * @return On success, the newly created process. On failure, `NULL` and `errno` is set.
 */
process_t* process_new(priority_t priority, group_member_t* group, namespace_t* ns);

/**
 * @brief Retrieves the process of the currently running thread.
 *
 * @note Will not increment the reference count of the returned process, as we consider the currently running thread to
 * always be referencing its process.
 *
 * @return The process of the currently running thread.
 */
static inline process_t* process_current(void)
{
    CLI_SCOPE();
    return _pcpu_sched->runThread->process;
}

/**
 * @brief Retrieves the process of the currently running thread without disabling interrupts.
 *
 * @note Will not increment the reference count of the returned process, as we consider the currently running thread to
 * always be referencing its process.
 *
 * @return The process of the currently running thread.
 */
static inline process_t* process_current_unsafe(void)
{
    return _pcpu_sched->runThread->process;
}

/**
 * @brief Gets a process by its ID.
 *
 * It is the responsibility of the caller to `UNREF()` the returned process.
 *
 * @param id The ID of the process to get.
 * @return A reference to the process with the specified ID or `NULL` if no such process exists.
 */
process_t* process_get(pid_t id);

/**
 * @brief Gets the namespace of a process.
 *
 * It is the responsibility of the caller to `UNREF()` the returned namespace.
 *
 * @param process The process to get the namespace of.
 * @return On success, a reference to the namespace of the process. On failure, `NULL` and `errno` is set:
 * - `EINVAL`: Invalid parameters.
 */
namespace_t* process_get_ns(process_t* process);

/**
 * @brief Sets the namespace of a process.
 *
 * @param process The process to set the namespace of.
 * @param ns The new namespace for the process.
 */
void process_set_ns(process_t* process, namespace_t* ns);

/**
 * @brief Kills a process, pushing it to the reaper.
 *
 * The process will still exist until the reaper removes it.
 *
 * @param process The process to kill.
 * @param status The exit status of the process.
 */
void process_kill(process_t* process, const char* status);

/**
 * @brief Removes a process from the system.
 *
 * This should only be called by the reaper.
 *
 * @param process The process to remove.
 */
void process_remove(process_t* process);

/**
 * @brief Gets the first thread of a process.
 *
 * @warning Must be used within a RCU read-side critical section.
 *
 * @param process The process to get the first thread of.
 * @return The first thread of the process, or `NULL` if the process has no threads.
 */
static inline thread_t* process_rcu_first_thread(process_t* process)
{
    return CONTAINER_OF_SAFE(list_first(&process->threads.list), thread_t, processEntry);
}

/**
 * @brief Gets the amount of threads in a process.
 *
 * @warning Must be used within a RCU read-side critical section.
 *
 * @param process The process to get the thread amount of.
 * @return The amount of threads in the process.
 */
static inline uint64_t process_rcu_thread_count(process_t* process)
{
    return process->threads.count;
}

/**
 * @brief Macro to iterate over all threads in a process.
 *
 * @warning Must be used within a RCU read-side critical section.
 *
 * @param thread Loop variable, a pointer to `thread_t`.
 * @param process The process to iterate the threads of.
 */
#define PROCESS_RCU_THREAD_FOR_EACH(thread, process) LIST_FOR_EACH(thread, &(process)->threads.list, processEntry)

/**
 * @brief Macro to iterate over all processes.
 *
 * @warning Must be used within a RCU read-side critical section.
 *
 * @param process Loop variable, a pointer to `process_t`.
 */
#define PROCESS_RCU_FOR_EACH(process) LIST_FOR_EACH(process, &_processes, entry)

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
 * Will never return `NULL` and will not increment the reference count of the returned process.
 *
 * @return The kernel process.
 */
process_t* process_get_kernel(void);

/** @} */
