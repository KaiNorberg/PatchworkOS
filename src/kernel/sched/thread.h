#pragma once

#include "cpu/simd.h"
#include "cpu/stack_pointer.h"
#include "cpu/syscalls.h"
#include "cpu/trap.h"
#include "ipc/note.h"
#include "proc/process.h"
#include "sched/sched.h"
#include "sched/wait.h"

#include <sys/list.h>
#include <sys/proc.h>

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
    THREAD_PARKED = 1 << 0,    ///< Is doing nothing, not in a queue, not blocking, think of it as "other".
    THREAD_READY = 1 << 1,     ///< Is ready to run and waiting to be scheduled.
    THREAD_RUNNING = 1 << 2,   ///< Is currently running on a cpu.
    THREAD_ZOMBIE = 1 << 3,    ///< Has exited and is waiting to be freed and might still be executing.
    THREAD_PRE_BLOCK = 1 << 4, ///< Has started the process of blocking but has not yet been given to a owner cpu.
    THREAD_BLOCKED = 1 << 5,   ///< Is blocking and waiting in one or multiple wait queues.
    THREAD_UNBLOCKING =
        1 << 6, ///< Has started unblocking, used to prevent the same thread being unblocked multiple times.
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
 * The kernel stack works the same way, but instead starts at the top of the higher half of the address space.
 *
 */
typedef struct thread
{
    list_entry_t entry;        ///< The list entry used by for example the scheduler and wait system.
    process_t* process;        ///< The parent process that the thread executes within.
    list_entry_t processEntry; ///< The list entry used by the parent process.
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
    sched_thread_ctx_t sched;
    wait_thread_ctx_t wait;
    simd_ctx_t simd;
    note_queue_t notes;
    syscall_ctx_t syscall;
    /**
     * The threads trap frame is used to save the values in the CPU registers such that the scheduler can continue
     * executing the thread later on.
     */
    trap_frame_t trapFrame;
} thread_t;

/**
 * @brief Retrieves the top of a threads kernel stack.
 *
 * Really just used in `start.s` to avoid having to access structs in assembly.
 *
 * @param thread The thread to query.
 * @return The address of the top of the kernel stack, this address is not inclusive.
 */
uint64_t thread_get_kernel_stack_top(thread_t* thread);

/**
 * @brief Creates a new thread structure.
 *
 * Does not push the created thread to the scheduler or similar, merely handling allocation and initialization.
 *
 * @param process The parent process that the thread will execute within.
 * @return On success, returns the newly created thread. On failure, returns `NULL` and `errno` is set.
 */
thread_t* thread_new(process_t* process);

/**
 * @brief Frees a thread structure.
 *
 * @param thread The thread to be freed.
 */
void thread_free(thread_t* thread);

/**
 * @brief Signals to a thread that it is dying.
 *
 * Does not perform free the thread and the thread will continue executing as a zombie after this function.
 *
 * @param thread The thread to be killed.
 */
void thread_kill(thread_t* thread);

/**
 * @brief Save state to a thread.
 *
 * @param thread The destination thread where the state will be saved.
 * @param trapFrame The source trapframe..
 */
void thread_save(thread_t* thread, const trap_frame_t* trapFrame);

/**
 * @brief Load state from a thread.
 *
 * Will retrieve the trap frame and setup the CPU with the threads contexts/data.
 *
 * @param thread The source thread to load state from.
 * @param trapFrame The destination trap frame.
 */
void thread_load(thread_t* thread, trap_frame_t* trapFrame);

/**
 * @brief Retrieve the trap frame from a thread.
 *
 * Will only retrieve the trap frame.
 *
 * @param thread The source thread.
 * @param trapFrame The destination trap frame.
 */
void thread_get_trap_frame(thread_t* thread, trap_frame_t* trapFrame);

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
 * deciding how critical the sent note is and unblocking the thread to notify it of the received note.
 *
 * @param thread The destination thread.
 * @param message The string of text to send to the thread, does not need to be NULL-terminated.
 * @param length The length of the string.
 * @return On success, returns 0. On failure, returns `ERR` and `errno` is set.
 */
uint64_t thread_send_note(thread_t* thread, const void* message, uint64_t length);

/**
 * @brief Retrieves the boot thread.
 *
 * The boot thread is the first thread created by the kernel. After boot it becomes the idle thread of the
 * bootstrap CPU. Is initialized lazily on the first call to this function, which should happen during early boot.
 *
 * Will never return `NULL`.
 *
 * @return The boot thread.
 */
thread_t* thread_get_boot(void);

/**
 * @brief Handles a page fault that occurred in the currently running thread.
 *
 * Called by the trap handler when a page fault occurs and allows the currently running thread to attempt to grow its
 * stacks if the faulting address is within one of its stack regions.
 *
 * @param trapFrame The trap frame containing the CPU state at the time of the page fault.
 * @return If the page fault was handled and the thread can continue executing, returns 0. If the thread must be killed,
 * `ERR` and `errno` is set.
 */
uint64_t thread_handle_page_fault(const trap_frame_t* trapFrame);

/** @} */
