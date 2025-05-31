#pragma once

#include "config.h"
#include "cpu/simd.h"
#include "cpu/syscall.h"
#include "cpu/trap.h"
#include "ipc/note.h"
#include "proc/process.h"
#include "sched/wait.h"

#include <errno.h>
#include <sys/list.h>
#include <sys/proc.h>

/**
 * @brief Thread of execution structure.
 * @ingroup kernel_sched
 * @defgroup kernel_sched_thread Thread
 * @struct thread_t
 *
 */

/**
 * @brief Thread priority type.
 * @ingroup kernel_sched_thread
 *
 * The `priority_t` type is used to store the scheduling priority of a thread. See `sched_schedule()` for more info.
 *
 */
typedef uint8_t priority_t;

/**
 * @brief Maximum thread priority constant.
 * @ingroup kernel_sched_thread
 *
 * The `PRIORITY_MAX` constant represents the maximum amount of priority levels for a thread, therefor the hightest
 * priority a thread can have is `PRIORITY_MAX` - 1. See `sched_schedule()` for more info.
 *
 */
#define PRIORITY_MAX 2

/**
 * @brief Minimum thread priority constant.
 * @ingroup kernel_sched_thread
 *
 * The `PRIORITY_MIN` constant represents the lowest priority a thread can have. See `sched_schedule()` for more info.
 *
 */
#define PRIORITY_MIN 0

/**
 * @brief Magic number to check for kernel stack overflow.
 * @ingroup kernel_sched_thread
 *
 * The `THREAD_CANARY` constant is a magic number stored att the bottom of a threads kernel stack, if a kernel stack
 * overflow were to occur this value would be modified, therefor we check the value of the canary in the
 * `trap_handler()` function to verify that hasent happened.
 *
 * Note that this kind of check is not fool proof, for example if a very large stack overflow would occur we would get
 * unpredictable behaviour as this would result in other modifications to the `thread_t` structure, however adding the
 * canary makes debugging far easier if a stack overflow were to occur and should catch the majority of overflows. If an
 * overflow in the kernel stack does occur increasing the value of `CONFIG_MAX_KERNEL_STACK` should fix the problem.
 *
 */
#define THREAD_CANARY 0x1A4DA90211FFC68CULL

/**
 * @brief Thread state enum.
 * @ingroup kernel_sched_thread
 *
 * The `thread_state_t` enum is used to prevent race conditions, reduce the need for locks by having the `thread_t`
 * structures state member be atomic, and to make debugging easier.
 *
 */
typedef enum
{
    THREAD_PARKED,     //!< Is doing nothing, not in a queue, not blocking, think of it as "other".
    THREAD_READY,      //!< Is ready to run and waiting to be scheduled.
    THREAD_RUNNING,    //!< Is currently running on a cpu.
    THREAD_ZOMBIE,     //!< Has exited and is waiting to be freed.
    THREAD_PRE_BLOCK,  //!< Has started the process of blocking put has not yet been given to a owner cpu, used to
                       //!< prevent race conditions when blocking on a lock.
    THREAD_BLOCKED,    //!< Is blocking and waiting in one or multiple wait queues.
    THREAD_UNBLOCKING, //!< Has started unblocking, used to prevent the same thread being unblocked multiple times.
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
     * @brief The time in clock cycles that the threads current time slice started.
     */
    clock_t timeStart;
    /**
     * @brief The time in clock cycles that the threads current time slice will end.
     */
    clock_t timeEnd;
    /**
     * @brief The scheduling priority of the thread.
     */
    priority_t priority;
    /**
     * @brief The current state of the thread.
     */
    _Atomic(thread_state_t) state;
    /**
     * @brief The last error that occoured while the thread was running, specified using errno codes.
     */
    errno_t error;
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
 * @var thread_t::entry
 * @brief The list entry used by for example the scheduler and wait system to store the thread in
 * linked lists and queues.
 */

/**
 * @var thread_t::process
 * @brief The parent process that the thread is executeing within.
 */

#define THREAD_KERNEL_STACK_TOP(thread) ((uintptr_t)thread->kernelStack + CONFIG_KERNEL_STACK)

#define THREAD_KERNEL_STACK_BOTTOM(thread) ((uintptr_t)thread->kernelStack)

thread_t* thread_new(process_t* process, void* entry, priority_t priority);

void thread_free(thread_t* thread);

void thread_save(thread_t* thread, const trap_frame_t* trapFrame);

void thread_load(thread_t* thread, trap_frame_t* trapFrame);

bool thread_is_note_pending(thread_t* thread);

uint64_t thread_send_note(thread_t* thread, const void* message, uint64_t length);
