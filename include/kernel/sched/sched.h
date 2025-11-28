#pragma once

#include <kernel/defs.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/rbtree.h>

#include <sys/list.h>
#include <sys/proc.h>

typedef struct process process_t;
typedef struct thread thread_t;

/**
 * @brief The Earliest Eligible Virtual Deadline First (EEVDF) scheduler.
 * @defgroup kernel_sched The Scheduler
 * @ingroup kernel
 *
 * PatchworkOS uses a scheduler inspired by the Earliest Eligible Virtual Deadline First (EEVDF) implementation used by
 * Linux.
 *
 * @see [Earliest Eligible Virtual Deadline
 * First](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=805acf7726282721504c8f00575d91ebfd750564) for
 * the original paper describing EEVDF (Might be slightly heavy reading, the other sources below are recommended).
 * @see [An EEVDF CPU scheduler for Linux](https://lwn.net/Articles/925371/) for the LWN article introducing EEVDF in
 * Linux.
 * @see [Completing the EEVDF Scheduler](https://lwn.net/Articles/969062/) for a LWN article containing additional
 * information on EEVDF.
 *
 * @{
 */

/**
 * @brief Per-thread scheduler context.
 * @struct sched_thread_ctx
 */
typedef struct sched_thread_ctx
{
    rbnode_t node;
    clock_t eligibleTime;     ///< The next time the thread will be eligible to run (its lag will be <= 0).
    clock_t virtualDeadline;  ///< The earliest time at which the thread will have received its fair share of CPU time.
    clock_t virtualRuntime;   ///< The total amount of virtual time the thread has actually received.
    clock_t virtualStartTime; ///< The time at which the thread was last scheduled (used to calculate how much virtual
                              ///< time to add).
    clock_t startTime; ///< The time at which the thread was created (used to calculate how much time it should have
                       ///< gotten).
    uint64_t timesScheduled; ///< The number of times the thread has been scheduled.
    uint64_t weight;         ///< The weight of the thread, derived from its process priority.
} sched_thread_ctx_t;

/**
 * @brief Per-CPU scheduler context.
 * @struct sched_cpu_ctx
 */
typedef struct sched_cpu_ctx
{
    rbtree_t runqueue;
    uint64_t totalWeight; ///< The total weight of all threads in the runqueue.
    lock_t lock;
    thread_t* idleThread;
    thread_t* runThread;
} sched_cpu_ctx_t;

/**
 * @brief Initialize the scheduler context for a thread.
 *
 * @param ctx The scheduler context to initialize.
 */
void sched_thread_ctx_init(sched_thread_ctx_t* ctx);

/**
 * @brief Initialize the scheduler context for a CPU.
 *
 * @param ctx The scheduler context to initialize.
 * @param cpu The CPU the context belongs to.
 */
void sched_cpu_ctx_init(sched_cpu_ctx_t* ctx);

/**
 * @brief Starts the scheduler by jumping to the boot thread.
 *
 * Will never return.
 *
 * @param bootThread The initial thread to switch to.
 */
_NORETURN void sched_start(thread_t* bootThread);

/**
 * @brief Sleeps the current thread for a specified duration in nanoseconds.
 *
 * @param timeout The duration to sleep in nanoseconds.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t sched_nanosleep(clock_t timeout);

/**
 * @brief Checks if the CPU is currently idle.
 *
 * @param cpu The CPU to check.
 * @return `true` if the CPU is idle, `false` otherwise.
 */
bool sched_is_idle(cpu_t* cpu);

/**
 * @brief Retrieves the currently running thread.
 *
 * @return The currently running thread.
 */
thread_t* sched_thread(void);

/**
 * @brief Retrieves the process of the currently running thread.
 *
 * @note Will not increment the reference count of the returned process, as we consider the currently running thread to
 * always be referencing its process.
 *
 * @return The process of the currently running thread.
 */
process_t* sched_process(void);

/**
 * @brief Retrieves the currently running thread without disabling interrupts.
 *
 * @return The currently running thread.
 */
thread_t* sched_thread_unsafe(void);

/**
 * @brief Retrieves the process of the currently running thread without disabling interrupts.
 *
 * @note Will not increment the reference count of the returned process, as we consider the currently running thread to
 * always be referencing its process.
 *
 * @return The process of the currently running thread.
 */
process_t* sched_process_unsafe(void);

/**
 * @brief Terminates the currently executing process and all its threads.
 *
 * @note Will never return, instead it triggers an interrupt that kills the current thread.
 *
 * @param status The exit status of the process.
 */
_NORETURN void sched_process_exit(uint64_t status);

/**
 * @brief Terminates the currently executing thread.
 *
 * @note Will never return, instead it triggers an interrupt that kills the thread.
 *
 */
_NORETURN void sched_thread_exit(void);

/**
 * @brief Pushes a thread onto a scheduling queue.
 *
 * @warning This will take ownership of the thread, so the caller should never access the pointer to the thread after
 * calling this function as if a preemption occurs it could be freed by the time the function returns.
 *
 * @param thread The thread to be pushed.
 * @param target The target cpu to push the thread to, or `NULL` for the current cpu.
 */
void sched_push(thread_t* thread, cpu_t* target);

/**
 * @brief Perform a scheduling operation.
 *
 * This function is called on every interrupt to provide a scheduling opportunity.
 *
 * @param frame The interrupt frame.
 * @param self The cpu performing the scheduling operation.
 */
void sched_do(interrupt_frame_t* frame, cpu_t* self);

/**
 * @brief The idle loop for the scheduler.
 *
 * This is where idle threads will run when there is nothing else to do.
 */
NORETURN extern void sched_idle_loop(void);

/** @} */
