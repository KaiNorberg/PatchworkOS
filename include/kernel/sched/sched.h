#pragma once

#include <_internal/config.h>
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
 * @defgroup kernel_sched The EEVDF Scheduler
 * @ingroup kernel
 *
 * The scheduler is implemented using the Earliest Eligible Virtual Deadline First (EEVDF) algorithm. EEVDF attempts to
 * give each thread a fair share of the CPU based on its weight by introducing the concepts of virtual time and virtual
 * deadlines. This is in contrast to more common algorithms that use fixed time slices or might rely on priority queues.
 *
 * Perhaps surprisingly, it's actually not that complex to implement. Everything is relative of course, but once you
 * understand the new concepts it introduces, its very elegant. So, included below is a brief explanation of each core
 * concept used by the EEVDF algorithm and some descriptions on how the scheduler works.
 *
 * ## Weight and Priority
 *
 * First, we need to assign each thread a "weight" based on the priority of its parent process. This weight is
 * calculated as
 *
 * ```
 * weight = process->priority + CONFIG_WEIGHT_BASE.
 * ```
 *
 * @note A higher value can be set for `CONFIG_WEIGHT_BASE` to reduce the significance of priority differences between
 * processes.
 *
 * Threads with a higher weight will receive a larger share of the available CPU time, specifically, the fraction of CPU
 * time a thread receives is proportional to its weight relative to the total weight of all active threads. This is
 * implemented using "virtual time", as described below.
 *
 * @see [EEVDF Paper](https://www.cs.utexas.edu/~pingali/CS380C/eevdf.pdf) page 2 for more information.
 *
 * ## Virtual Time
 *
 * The EEVDF algorithm introduces the concept of "virtual time", this is the mechanism that tracks how much CPU time
 * each thread ought to receive. Each scheduler maintains a "virtual clock" that runs at a rate inversely proportional
 * to the `totalWeight` of all active threads. So, if the total weight is `10`, then each unit of virtual time
 * corresponds to `10` units of real time.
 *
 * Each thread should receive an amount of real CPU time equal to its weight for each virtual time unit that passes. For
 * example, if we have two threads A and B with weights `2` and `3` respectively, then for every `1` unit of virtual
 * time that passes, thread A should receive `2` units of real CPU time and thread B should receive `3` units of real
 * CPU time.
 *
 * @note All variables storing virtual time values will be prefixed with 'v' and use the `vclock_t` type. Variables
 * storing real time values will use the `clock_t` type as normal.
 *
 * @see [EEVDF Paper](https://www.cs.utexas.edu/~pingali/CS380C/eevdf.pdf) pages 8-9 for more information.
 *
 * ## Lag
 *
 * As the name "Earliest Eligible Virtual Deadline First" suggests, there are two main concepts that determine which
 * thread to run, its "eligibility" and its "virtual deadline". We will start with "eligibility", which is determined by
 * the concept of "lag".
 *
 * Lag is defined as the difference between the amount of real CPU time a thread should have received and the amount of
 * real CPU time it has actually received.
 *
 * As an example, lets say we have three threads A, B and C with equal weights. To start with each thread is supposed to
 * have run for 0ms, and has actually run for 0ms:
 *
 * ```
 * Thread | Lag (ms)
 * -------|-------
 *    A   |   0
 *    B   |   0
 *    C   |   0
 * ```
 *
 * Now, lets say we give a 30ms (in real time) time slice to thread A. The lag values will now be:
 *
 * ```
 * Thread | Lag (ms)
 * -------|-------
 *    A   |  -20
 *    B   |   10
 *    C   |   10
 * ```
 *
 * What just happened is that each thread should have received one third of the CPU time (since they are all of equal
 * weight such that each of their weights is 1/3 of the total weight) which is 10ms. Therefore, since thread A actually
 * received 30ms of CPU time, it has run for 20ms more than it should have. Meanwhile, threads B and C have not received
 * any CPU time, such that they have received 10ms less than they should have. Note that the sum of all lag values is
 * always zero.
 *
 * A thread is considered eligible if, and only if, its lag is greater than or equal to zero. In the above example
 * threads B and C are eligible to run, while thread A is not.
 *
 * A property of lag is that the sum of all lag values across all active threads is always zero.
 *
 * @note Fairness is achieved over some long period of time, over which the proportion of CPU time each thread has
 * received will converge to the share it ought to receive, not that each individual time slice is exactly correct,
 * which is why thread A was allowed to run for 30ms.
 *
 * ## Virtual Deadlines
 *
 * Lets now move on to the other part of the name, "virtual deadlines". The goal of the scheduler is to always run the
 * eligible thread with the earliest virtual deadline, as the name suggests. So, what is a virtual deadline?
 *
 * A virtual deadline is defined as the earliest time at which a thread should have received its due share of CPU time.
 * Which is determined as the sum of the virtual time at which the thread becomes eligible and the amount of virtual
 * time corresponding to the thread's next time slice.
 *
 * From the description of lag above, we can see that the virtual time at which thread becomes eligible is simply the
 * virtual time at which its lag becomes non-negative. In order to determine that, its important to know that to covert
 * from real time to virtual time, we divide the real time by the total weight of all active threads. Therefore, written
 * in a simplified form, the virtual deadline can be calculated as:
 *
 * ```
 * vdeadline = veligible + vtimeSlice = (vclock - lag / totalWeight) + (timeSlice / totalWeight)
 * ```
 *
 * where `vclock` is the current virtual time of the scheduler, `lag` is the lag of the thread in real time,
 * `totalWeight` is the total weight of all active threads, and `timeSlice` is the length of the next time slice for the
 * thread in real time.
 *
 * ## Entering and Leaving the Scheduler
 *
 * An issue arises when a thread enters or leaves the scheduler (e.g. when a thread is created, exists, blocks or
 * unblocks). In such cases we need to ensure that the lag and virtual clock remains consistent. To achieve this, when a
 * thread enters the scheduler, we adjust the scheduler's virtual clock by subtracting the thread's lag converted to
 * virtual time. Conversely, when a thread leaves the scheduler, we adjust the scheduler's virtual clock by adding the
 * thread's lag converted to virtual time. The proof for this is outside the scope of this documentation, but it can be
 * found in the EEVDF paper.
 *
 * @see [EEVDF Paper](https://www.cs.utexas.edu/~pingali/CS380C/eevdf.pdf) pages 10-11 for more information.
 *
 * ## Scheduling
 *
 * With the central concepts introduced, we can now describe how the scheduler works. As mentioned, the goal is to
 * always run the eligible thread with the earliest virtual deadline. To achieve this, each scheduler maintains a
 * runqueue in the form of a Red-Black tree sorted by virtual deadline.
 *
 * To select the next thread to run, we find the first eligible thread in the runqueue and switch to it. If no eligible
 * thread is found, we switch to the idle thread. Which is a special thread that is not considred active and simply runs
 * an infinite loop that halts the CPU while waiting for an interrupt.
 *
 * ## Load Balancing
 *
 * Each CPU has its own scheduler and associated runqueue, as such we need to balance the load between each CPU. To
 * accomplish this, we run a check before any scheduling opportunity such that if a scheduler's neighbor CPU has a
 * `CONFIG_LOAD_BALANCE_BIAS` number of threads fewer than itself, it will push its thread with the highest virtual
 * deadline to the neighbor CPU.
 *
 * @note The reason we want to avoid a global runqueue is to avoid lock contention, but also to reduce cache misses by
 * keeping threads on the same CPU when reasonably possible.
 *
 * TODO: The load balancing algorithm is rather naive at the moment and could be improved in the future.
 *
 * ## Testing
 *
 * The scheduler is tested using primarily asserts and additional checks in debug builds.
 *
 * @see [Earliest Eligible Virtual Deadline
 * First](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=805acf7726282721504c8f00575d91ebfd750564) for
 * the original paper describing EEVDF.
 * @see [An EEVDF CPU scheduler for Linux](https://lwn.net/Articles/925371/) for the LWN article introducing EEVDF in
 * Linux.
 * @see [Completing the EEVDF Scheduler](https://lwn.net/Articles/969062/) for a LWN article containing additional
 * information on EEVDF.
 *
 * @{
 */

/**
 * @brief Virtual clock type.
 * @typedef vclock_t
 */
typedef int64_t vclock_t;

/**
 * @brief Lag type.
 * @struct lag_t
 */
typedef int64_t lag_t;

/**
 * @brief Virtual Lag type.
 * @struct vlag_t
 */
typedef int64_t vlag_t;

/**
 * @brief Per-thread scheduler context.
 * @struct sched_client_t
 */
typedef struct sched_client
{
    list_entry_t activeEntry; ///< Entry in the CPU's active thread list, used for debugging.
    rbnode_t node;            ///< Node in the scheduler's runqueue.
    int64_t weight;
    vclock_t vdeadline;
    vclock_t veligibleTime;
    vclock_t vstart;
    clock_t runtime;
    clock_t timeSliceStart;
    clock_t timeSliceEnd;
    lag_t cachedLag; 
} sched_client_t;

/**
 * @brief Per-CPU scheduler.
 * @struct sched_t
 */
typedef struct sched
{
    list_t activeThreads; ///< List of all active threads on this CPU, used for debugging.
    int64_t totalWeight; ///< The total weight of all active threads.
    rbtree_t runqueue;    ///< Contains all runnable threads, sorted by virtual deadline.
    vclock_t vtimeRemainder;
    vclock_t vtime; ///< The current virtual time of the CPU.
    clock_t lastUpdate;   ///< Uptime when the last vtime update occurred.
    lock_t lock;
    thread_t* idleThread;
    thread_t* runThread;
} sched_t;

/**
 * @brief Initialize the scheduler context for a thread.
 *
 * @param client The scheduler context to initialize.
 */
void sched_client_init(sched_client_t* client);

/**
 * @brief Initialize the scheduler for a CPU.
 *
 * @param sched The scheduler to initialize.
 */
void sched_init(sched_t* sched);

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
 * @brief Terminates the currently executing process and all it's threads.
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
 * @brief Yield the current thread's time slice to allow other threads to run.
 */
void sched_yield(void);

/**
 * @brief Submits a thread to be scheduled on the current CPU.
 *
 * @param thread The thread to submit.
 * @param target The target CPU to schedule the thread on, or `NULL` for the current CPU.
 */
void sched_submit(thread_t* thread, cpu_t* target);

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
_NORETURN extern void sched_idle_loop(void);

/** @} */
