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
 * ## Thread Stacks
 * The position of a thread user stack is decided based on its thread id. The user stack of the thread with id 0 is
 * located at the top of the lower half of the address space, the user stack is `CONFIG_MAX_USER_STACK_PAGES` pages
 * long, and below it is the guard page. Below that is the user stack of the thread with id 1, below that is its guard
 * page, it then continues like that for however many threads there are.
 *
 * The kernel stack works the same way, but instead starts just under the kernel code and data section, at the top of
 * the kernel stacks region and each stack is `CONFIG_MAX_KERNEL_STACK_PAGES` pages long.
 *
 */
typedef struct thread
{
    process_t* process;        ///< The parent process that the thread executes within.
    list_entry_t processEntry; ///< The entry for the parent process.
    tid_t id;                  ///< The thread id, unique within a `process_t`.
    /**
     * The current state of the thread, used to prevent race conditions and make debugging easier.
     */
    _Atomic(thread_state_t) state;
    /**
     * The last error that occurred while the thread was running, specified using errno codes.
     */
    errno_t error;
    stack_pointer_t kernelStack; ///< The kernel stack of the thread.
    stack_pointer_t userStack;   ///< The user stack of the thread.
    sched_client_t sched;
    wait_client_t wait;
    simd_ctx_t simd;
    note_queue_t notes;
    syscall_ctx_t syscall;
    perf_thread_ctx_t perf;
    rcu_entry_t rcu;
    uintptr_t fsBase; ///< The FS base address for the thread.
    /**
     * The threads interrupt frame is used to save the values in the CPU registers such that the scheduler can continue
     * executing the thread later on.
     */
    interrupt_frame_t frame;
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
status_t thread_kernel_create(thread_kernel_entry_t entry, void* arg, tid_t* out);

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
 * @brief Safely copy data from user space.
 *
 * Will pin the user pages in memory while performing the copy and expand the user stack if necessary.
 *
 * @param thread The thread performing the operation.
 * @param dest The destination buffer in kernel space.
 * @param userSrc The source buffer in user space.
 * @param length The number of bytes to copy.
 * @return An appropriate status value.
 */
status_t thread_copy_from_user(thread_t* thread, void* dest, const void* userSrc, uint64_t length);

/**
 * @brief Safely copy data to user space.
 *
 * Will pin the user pages in memory while performing the copy and expand the user stack if necessary.
 *
 * @param thread The thread performing the operation.
 * @param userDest The destination buffer in user space.
 * @param src The source buffer in kernel space.
 * @param length The number of bytes to copy.
 * @return An appropriate status value.
 */
status_t thread_copy_to_user(thread_t* thread, void* userDest, const void* src, uint64_t length);

/**
 * @brief Safely copy a null-terminated array of objects from user space.
 *
 * @param thread The thread performing the operation.
 * @param userArray The source array in user space.
 * @param terminator A pointer to the terminator object.
 * @param objectSize The size of each object in the array.
 * @param maxCount The maximum number of objects to copy.
 * @param outArray Output pointer to store the allocated array in kernel space, must be freed by the caller.
 * @param outCount Output pointer to store the number of objects copied, can be `NULL`.
 * @return An appropriate status value.
 */
status_t thread_copy_from_user_terminated(thread_t* thread, const void* userArray, const void* terminator,
    uint8_t objectSize, uint64_t maxCount, void** outArray, uint64_t* outCount);

/**
 * @brief Safely copy a string from user space.
 *
 * @param thread The thread performing the operation.
 * @param dest The destination buffer in kernel space.
 * @param userSrc The source buffer in user space.
 * @param size The size of the destination buffer.
 * @return An appropriate status value.
 */
status_t thread_copy_from_user_string(thread_t* thread, char* dest, const char* userSrc, uint64_t size);

/**
 * @brief Safely copy a string from user space and use it to initialize a pathname.
 *
 * @param thread The thread performing the operation.
 * @param pathname A pointer to the pathname to initialize.
 * @param userPath The string in user space.
 * @return An appropriate status value.
 */
status_t thread_copy_from_user_pathname(thread_t* thread, pathname_t* pathname, const char* userPath);

/**
 * @brief Safely copy a null-terminated array of strings and their contents from user space into a string vector.
 *
 * @param thread The thread performing the operation.
 * @param user The source array of strings in user space.
 * @param out Output pointer to store the allocated array of strings in kernel space, must be freed by the caller.
 * @param outAmount Output pointer to store the number of strings copied, can be `NULL`.
 * @return An appropriate status value.
 */
status_t thread_copy_from_user_string_array(thread_t* thread, const char** user, char*** out, uint64_t* outAmount);

/**
 * @brief Atomically load a 64-bit value from a user-space atomic variable.
 *
 * Will pin the user pages in memory while performing the load and expand the user stack if necessary.
 *
 * @param thread The thread performing the operation.
 * @param userObj The user-space atomic variable to load from.
 * @param outValue Output pointer to store the loaded value.
 * @return An appropriate status value.
 */
status_t thread_load_atomic_from_user(thread_t* thread, atomic_uint64_t* userObj, uint64_t* outValue);

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
