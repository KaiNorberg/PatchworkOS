#pragma once

#include "config.h"
#include "cpu/simd.h"
#include "cpu/syscalls.h"
#include "cpu/trap.h"
#include "ipc/note.h"
#include "proc/process.h"
#include "sched/sched.h"
#include "sched/wait.h"

#include <errno.h>
#include <sys/list.h>
#include <sys/proc.h>

/**
 * @brief Thread of execution structure.
 * @ingroup kernel_sched
 * @defgroup kernel_sched_thread Thread
 *
 */

/**
 * @brief Magic number to check for kernel stack overflow.
 * @ingroup kernel_sched_thread
 * @def THREAD_CANARY
 *
 * The `THREAD_CANARY` constant is a magic number stored att the bottom of a threads kernel stack, if the kernel stack
 * were to overflow this value would be modified, therefor we check the value of the canary in the every now and then.
 *
 * Note that this kind of check is not fool proof, for example if a very large stack overflow were occur we would get
 * unpredictable behaviour as this would result in other modifications to the `thread_t` structure, however adding the
 * canary makes debugging far easier if a stack overflow were to occur and should catch the majority of overflows. If an
 * overflow in the kernel stack does occur increasing the value of `CONFIG_MAX_KERNEL_STACK` should fix the problem.
 *
 */
#define THREAD_CANARY 0x1A4DA90211FFC68CULL

/**
 * @brief Thread state enum.
 * @ingroup kernel_sched_thread
 * @enum thread_state_t
 *
 * The `thread_state_t` enum is used to prevent race conditions, reduce the need for locks by having the `thread_t`
 * structures state member be atomic, and to make debugging easier.
 *
 */
typedef enum
{
    THREAD_PARKED = 1 << 0,    //!< Is doing nothing, not in a queue, not blocking, think of it as "other".
    THREAD_READY = 1 << 1,     //!< Is ready to run and waiting to be scheduled.
    THREAD_RUNNING = 1 << 2,   //!< Is currently running on a cpu.
    THREAD_ZOMBIE = 1 << 3,    //!< Has exited and is waiting to be freed.
    THREAD_PRE_BLOCK = 1 << 4, //!< Has started the process of blocking put has not yet been given to a owner cpu, used
                               //!< to prevent race conditions when blocking on a lock.
    THREAD_BLOCKED = 1 << 5,   //!< Is blocking and waiting in one or multiple wait queues.
    THREAD_UNBLOCKING =
        1 << 6, //!< Has started unblocking, used to prevent the same thread being unblocked multiple times.
} thread_state_t;

/**
 * @brief Thread of execution structure.
 * @ingroup kernel_sched_thread
 * @struct thread_t
 *
 * The `thread_t` structure stores information like a trap frame, kernel stack, and scheduling information amongst other
 * things, it is the fundamental unit of execution managed by the kernel's scheduler, meaning that it encapsulates all
 * the necessary context for the kernel to save, restore, and schedule the thread for execution on a CPU. Each
 * `thread_t` represents an independent thread of execution within a `process_t`.
 *
 */
typedef struct thread
{
    /**
     * @brief The list entry used by for example the scheduler and wait system to store the thread in
     * linked lists and queues.
     */
    list_entry_t entry;
    /**
     * @brief The parent process that the thread is executeing within.
     */
    process_t* process;
    /**
     * @brief The list entry used by the parent process to store the thread.
     */
    list_entry_t processEntry;
    /**
     * @brief A identifier that is unique to every thread in a process, separate processes may contain threads that
     * overlap.
     */
    tid_t id;
    /**
     * @brief The current state of the thread, used to prevent race conditions and make debugging easier.
     */
    _Atomic(thread_state_t) state;
    /**
     * @brief The last error that occoured while the thread was running, specified using errno codes.
     */
    errno_t error;
    /**
     * @brief The threads sched context, for more info check the type description.
     */
    sched_thread_ctx_t sched;
    /**
     * @brief The threads wait context, for more info check the type description.
     */
    wait_thread_ctx_t wait;
    /**
     * @brief The threads simd context, for more info check the type description.
     */
    simd_ctx_t simd;
    /**
     * @brief The threads note queue context, for more info check the type description.
     */
    note_queue_t notes;
    /**
     * @brief The threads syscall context, for more info check the type description.
     */
    syscall_ctx_t syscall;
    /**
     * @brief The threads trap frame is used to save the value in various CPU registers such that the
     * scheduler can continue executing the thread later on.
     */
    trap_frame_t trapFrame;
    /**
     * @brief The threads canary, for more info see `THREAD_CANARY`.
     */
    uint64_t canary;
    /**
     * @brief The threads kernel stack, stored as part of the `thread_t` structure in
     * order to avoid an additional allocation when allocating a new thread.
     */
    uint8_t kernelStack[CONFIG_KERNEL_STACK];
} thread_t;

/**
 * @brief Retrieves the top of a threads kernel stack.
 * @ingroup kernel_sched_thread
 * @def THREAD_KERNEL_STACK_TOP
 *
 * The `THREAD_KERNEL_STACK_TOP()` macro retrieves the address of the top of a threads kernel stack.
 *
 * Note that in x86 the push operation moves the stack pointer first and then writes to the location of the stack
 * pointer, that means that even if this address is not inclusive the stack pointer of a thread should be initalized to
 * the result of the `THREAD_KERNEL_STACK_TOP()` macro.
 *
 * @return The address of the top of the kernel stack, this address is not inclusive and always page aligned.
 */
#define THREAD_KERNEL_STACK_TOP(thread) ((uintptr_t)thread->kernelStack + CONFIG_KERNEL_STACK)

/**
 * @brief Retrieves the bottom of a threads kernel stack.
 * @ingroup kernel_sched_thread
 * @def THREAD_KERNEL_STACK_BOTTOM
 *
 * The `THREAD_KERNEL_STACK_BOTTOM()` macro retrieves the address of the bottom of a threads kernel.
 *
 * @return The address of the bottom of the kernel stack, this address is inclusive and always page aligned.
 */
#define THREAD_KERNEL_STACK_BOTTOM(thread) ((uintptr_t)thread->kernelStack)

/**
 * @brief Creates a new thread structure.
 * @ingroup kernel_sched_thread
 *
 * The `thread_new()` function allocates and initializes a `thread_t` structure, it does not push the created thread to
 * the scheduler or similar, merely handling allocation and initialization.
 *
 * @param process The parent process that the thread will execute within.
 * @param entry The inital value of the threads rip register, defines where the thread will start executing code.
 * @return On success, returns the newly created thread. On failure, returns `NULL`.
 */
thread_t* thread_new(process_t* process, void* entry);

/**
 * @brief Frees a thread structure.
 * @ingroup kernel_sched_thread
 *
 * The `thread_free()` function deinitializes a `thread_t` structure and frees it.
 *
 * @param thread The thread to be freed.
 */
void thread_free(thread_t* thread);

/**
 * @brief Save state to thread.
 * @ingroup kernel_sched_thread
 *
 * The `thread_save()` function saves current CPU state and trap frame to the `thread_t` structure allowing it to be
 * rescheduled later on. Examples of CPU state that is saved is the SIMD context.
 *
 * @param thread The destination thread where the state will be saved.
 * @param trapFrame The source trapframe storing register state.
 */
void thread_save(thread_t* thread, const trap_frame_t* trapFrame);

/**
 * @brief Load state from thread.
 * @ingroup kernel_sched_thread
 *
 * The `thread_load()` function loads state information from a `thread_t` structure, including but not limited to, the
 * trap frame, SIMD context, address space, and task state segment.
 *
 * @param thread The source thread to load state from.
 * @param trapFrame The destination trap frame to load register state.
 */
void thread_load(thread_t* thread, trap_frame_t* trapFrame);

/**
 * @brief Check if a thread has a note pending.
 * @ingroup kernel_sched_thread
 *
 * @param thread The thread to query.
 * @return True if there is a note pending, false otherwise.
 */
bool thread_is_note_pending(thread_t* thread);

/**
 * @brief Send a note to a thread.
 * @ingroup kernel_sched_thread
 *
 * The `thread_send_note()` function sends a note to a thread using the `note_queue_push()` function, this function
 * should always be used over the `note_queue_push()` function, as it performs additional checks, like deciding how
 * critical the sent note is and unblocking the thread to notify it of the received note.
 *
 * @param thread The destination thread.
 * @param message The string of text to send to the thread, does not need to be NULL-terminated.
 * @param length The length of the string.
 * @return On success, returns 0. On failure, returns `ERR`.
 */
uint64_t thread_send_note(thread_t* thread, const void* message, uint64_t length);
