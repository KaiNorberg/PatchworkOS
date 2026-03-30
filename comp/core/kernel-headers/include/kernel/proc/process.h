#pragma once

#include <kernel/drivers/perf.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file_table.h>
#include <kernel/io/ioring.h>
#include <kernel/ipc/note.h>
#include <kernel/mem/space.h>
#include <kernel/proc/job.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/rcu.h>
#include <kernel/utils/ref.h>

#include <libc/map.h>
#include <libc/status.h>
#include <stdatomic.h>

/**
 * @brief Process management.
 * @defgroup kernel_proc Process Subsystem
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
} process_flags_t;

/**
 * @brief Represents the threads in a process.
 * @struct process_threads_t
 */
typedef struct
{
    _Atomic(thrd_t) newTid;
    list_t list; ///< Reads are RCU protected, writes require the lock.
    uint64_t count;
    lock_t lock;
} process_threads_t;

/**
 * @brief Maximum length of a process exit result.
 */
#define PROCESS_RESULT_MAX 256

/**
 * @brief Process exit result structure.
 * @struct process_result_t
 *
 * The term "exit result" is used over the more common "exit status" to avoid confusion with the `status_t` type.
 */
typedef struct
{
    char buffer[PROCESS_RESULT_MAX];
    lock_t lock;
} process_result_t;

/**
 * @brief Process arguments structure.
 * @struct process_args_t
 */
typedef struct
{
    lock_t lock;
    char* buffer;
    size_t length;
} process_args_t;

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
    proc_t id;
    _Atomic(prio_t) priority;
    process_result_t result;
    space_t space;
    file_table_t files;
    perf_process_ctx_t perf;
    ioring_ctx_t rings[CONFIG_MAX_RINGS];
    note_handler_t noteHandler;
    list_t dyingIrps;
    lock_t dyingIrpsLock;
    _Atomic(process_flags_t) flags;
    process_threads_t threads;
    process_args_t args;
    clock_t start;
    job_member_t job;
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
 * @param out Output pointer to store the new process.
 * @param priority The priority of the new process.
 * @param job The job to add the new process to.
 * @return An appropriate status value.
 */
status_t process_new(process_t** out, prio_t priority, job_t* job);

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
process_t* process_get(proc_t id);

/**
 * @brief Kills a process by sending kill notes to all its threads.
 *
 * @param process The process to kill.
 * @param result The exit result of the process.
 */
void process_kill(process_t* process, const char* result);

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
 * @brief Sends a note to a process.
 *
 * The note will be delivered to one of the process's threads.
 *
 * @param process The destination process.
 * @param note The note string to send.
 * @return An appropriate status value.
 */
status_t process_send_note(process_t* process, const char* note);

/**
 * @brief Checks if a process has a thread with the specified thread ID.
 *
 * @param process The process to check.
 * @param tid The thread ID to look for.
 * @return `true` if the process has a thread with the specified ID, `false` otherwise.
 */
bool process_has_thread(process_t* process, thrd_t tid);

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
