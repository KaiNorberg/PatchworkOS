#pragma once

#include <kernel/cpu/gdt.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/simd.h>
#include <kernel/cpu/stack_pointer.h>
#include <kernel/cpu/syscall.h>
#include <kernel/drivers/perf.h>
#include <kernel/fs/path.h>
#include <kernel/ipc/note.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/rcu.h>
#include <kernel/utils/ref.h>

#include <sys/list.h>
#include <sys/proc.h>
#include <sys/status.h>
#include <threads.h>

typedef struct process process_t;
typedef struct thread thread_t;

/**
 * @brief Thread of execution.
 * @ingroup kernel_sched
 * @defgroup kernel_sched_thread Threads
 * @{
 */

/**
 * @brief Thread state enum.
 * @enum thread_state_t
 *
 */
typedef enum
{
    THREAD_PARKED = 0, ///< Is doing nothing, not in a queue, not blocking, think of it as "other".
    THREAD_ACTIVE,     ///< Is either running or ready to run.
    THREAD_PRE_BLOCK,  ///< Has started the process of blocking but has not yet been given to a owner cpu.
    THREAD_BLOCKED,    ///< Is blocking and waiting in one or multiple wait queues.
    THREAD_UNBLOCKING, ///< Has started unblocking, used to prevent the same thread being unblocked multiple times.
    THREAD_DYING,      ///< The thread is currently dying, it will be freed by the scheduler once its invoked.
} thread_state_t;

/**
 * @brief Thread of execution structure.
 * @struct thread_t
 *
 * A `thread_t` represents an independent thread of execution within a `process_t`.
 *
 */
typedef struct thread
{
    process_t* process;        ///< The parent process that the thread executes within.
    list_entry_t processEntry; ///< The entry for the parent process.
    thrd_t id;                 ///< The thread id, unique within a `process_t`.
    /**
     * The current state of the thread, used to prevent race conditions and make debugging easier.
     */
    _Atomic(thread_state_t) state;
    /**
     * The last error that occurred while the thread was running, specified using errno codes.
     */
    sched_client_t sched;
    wait_client_t wait;
    simd_ctx_t simd;
    note_queue_t notes;
    syscall_ctx_t syscall;
    perf_thread_ctx_t perf;
    uintptr_t fsBase; ///< The FS base address for the thread.
    /**
     * The threads interrupt frame is used to save the values in the CPU registers such that the scheduler can continue
     * executing the thread later on.
     */
    interrupt_frame_t frame;
    rcu_entry_t rcu;
    stack_pointer_t kernelStack;
    uint8_t kernelStackBuffer[CONFIG_KERNEL_STACK_PAGES * PAGE_SIZE] ALIGNED(64);
} thread_t;

/**
 * @brief Creates a new thread structure.
 *
 * Does not push the created thread to the scheduler or similar, merely handling allocation and initialization.
 *
 * @param out Output pointer for the thread.
 * @param process The parent process that the thread will execute within.
 * @return An appropriate status value.
 */
status_t thread_new(thread_t** out, process_t* process);

/**
 * @brief Frees a thread structure.
 *
 * @param thread The thread to be freed.
 */
void thread_free(thread_t* thread);

/**
 * @brief Kernel thread entry point function type.
 */
typedef void (*thread_kernel_entry_t)(void* arg);

/**
 * @brief Creates a new thread that runs in kernel mode and submits it to the scheduler.
 *
 * @param entry The entry point function for the thread.
 * @param arg An argument to pass to the entry point function.
 * @param out Output pointer to store the thread ID, can be `NULL`.
 * @return An appropriate status value.
 */
status_t thread_kernel_create(thread_kernel_entry_t entry, void* arg, thrd_t* out);

/**
 * @brief Retrieves the currently running thread.
 *
 * @return The currently running thread.
 */
static inline thread_t* thread_current(void)
{
    CLI_SCOPE();
    return _pcpu_sched->runThread;
}

/**
 * @brief Retrieves the currently running thread without disabling interrupts.
 *
 * @return The currently running thread.
 */
static inline thread_t* thread_current_unsafe(void)
{
    return _pcpu_sched->runThread;
}

/**
 * @brief Retrieves the idle thread for the current CPU.
 *
 * @return The idle thread for the current CPU.
 */
static inline thread_t* thread_idle(void)
{
    CLI_SCOPE();
    return _pcpu_sched->idleThread;
}

/**
 * @brief Retrieves the idle thread for the current CPU without disabling interrupts.
 *
 * @return The idle thread for the current CPU.
 */
static inline thread_t* thread_idle_unsafe(void)
{
    return _pcpu_sched->idleThread;
}

/**
 * @brief Save state to a thread.
 *
 * @param thread The destination thread where the state will be saved.
 * @param frame The source frame..
 */
void thread_save(thread_t* thread, const interrupt_frame_t* frame);

/**
 * @brief Load state from a thread.
 *
 * Will retrieve the interrupt frame and setup the CPU with the threads contexts/data.
 *
 * @param thread The source thread to load state from.
 * @param frame The destination interrupt frame.
 */
void thread_load(thread_t* thread, interrupt_frame_t* frame);

/**
 * @brief Check if a thread has a note pending.
 *
 * @param thread The thread to query.
 * @return True if there is a note pending, false otherwise.
 */
bool thread_is_note_pending(thread_t* thread);

/**
 * @brief Send a note to a thread.
 *
 * This function should always be used over the `note_queue_push()` function, as it performs additional checks, like
 * unblocking the thread to notify it of the received note.
 *
 * @param thread The destination thread.
 * @param string The note string to send, should be a null-terminated string.
 * @return An appropriate status value.
 */
status_t thread_send_note(thread_t* thread, const char* string);

/**
 * @brief Jump to a thread by calling `thread_load()` and then loading its interrupt frame.
 *
 * Must be done in assembly as it requires directly modifying registers.
 *
 * Will never return instead it ends up at `thread->frame.rip`.
 *
 * @param thread The thread to jump to.
 */
_NORETURN extern void thread_jump(thread_t* thread);

/** @} */
