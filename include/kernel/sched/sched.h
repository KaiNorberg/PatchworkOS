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
 * @defgroup kernel_sched The EEVDF Scheduler
 * @ingroup kernel
 *
 * The scheduler is implemented using the Earliest Eligible Virtual Deadline First (EEVDF) algorithm. EEVDF attempts to give each thread a fair share of the CPU based on its weight by using virtual time and virtual deadlines. Perhaps surprisingly, its actually not that complex to implement, once you understand the new concepts it introduces.
 *
 * ## Weight and Priority
 *
 * Each thread is assigned a "weight" based on the priority of its parent process. This weight is calculated as
 *
 * ```
 * weight = process->priority + CONFIG_WEIGHT_BASE.
 * ```
 *
 * @note A higher value can be set for `CONFIG_WEIGHT_BASE` to reduce the significance of priority differences between
 * processes.
 *
 * Threads with a higher weight will receive a larger share of the available CPU time, specifically, the fraction of CPU
 * time a thread receives is proportional to its weight relative to the total weight of all runnable threads.
 *
 * ## Virtual Time
 *
 * The EEVDF algorithm introduces the concept of "virtual time". Each thread has a virtual clock that runs at a rate proportional to its weight, such that higher weight threads have their virtual clocks run slower. 
 *
 * This is how each thread's share of the CPU is determined. From the perspective of virtual time, all threads should receive an equal amount of virtual CPU time. However, since each virtual clock runs at a different rate, this translates to different amounts of real CPU time for each thread.
 * 
 * The total amount of virtual time a thread has ran for is tracked using the `vruntime` variable. This variable will be important later on.
 * 
 * To convert a duration in real time to virtual time, we use the formula
 * 
 * ```
 * vclock = (clock * CONFIG_WEIGHT_BASE) / weight.
 * ```
 * 
 * Where `vclock` is the duration in virtual time, `clock` is the duration in real time, and `weight` is the weight of the thread.
 * 
 * @note All variables storing virtual time values will be prefixed with 'v' and use the `vclock_t` type. Variables storing real time values will use the `clock_t` type as normal.
 * 
 * ## Lag
 * 
 * The key metric used to determine what thread to schedule next is called "lag". Lag is defined as the difference between the amount of CPU time a thread should have received, and the amount of CPU time it has actually received.
 * 
 * @note Since lag is calculated using virtual time, the amount of CPU time each thread should have received will appear to always be equal.
 * 
 * As an example, lets say we have three threads A, B and C with equal weights. To start with each thread is supposed to have run for 0ms, and has actually run for 0ms:
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
 * What just happened is that each thread should have received one third of the CPU time  (since they are all of equal weight such that each of their weights is 1/3 of the total weight) which is 10ms. Therefore, since thread A actually received 30ms of CPU time, it has run for 20ms more than it should have, while threads B and C have not received any CPU time, such that they are both 10ms behind where they should be. Additionally, note the property that the sum of all lag values is always zero.
 * 
 * @note Fairness is achieved such that over some long period of time, the proportion of CPU time each thread receives will converge to the share it ought to receive, not that each individual time slice is exactly correct, which is why thread A was allowed to run for 30ms.
 * 
 * ## Virtual Deadlines
 * 
 * To determine which thread to schedule next, we could use lag directly. However, as will be shown, there is a far simpler approach. Instead of lag, EEVDF introduces the concept of "virtual deadlines". A virtual deadline is defined as the point in virtual time at which a thread is expected to finish its next time slice. The virtual deadline is calculated as:
 * 
 * ```
 * vdeadline = vruntime + vtimeSlice
 * ```
 * 
 * Where `vtimeSlice` is the length of the time slice in virtual time. So how does this relate to lag? Consider that, by definition, lag can be expressed as
 * 
 * ```
 * vruntime = expectedVruntime - lag.
 * ```
 * 
 * Where `expectedVruntime` is how much virtual time the thread should have ran for. Therefore, substituting this into our equation for virtual deadlines we get
 * 
 * ```
 * vdeadline = (expectedVruntime - lag) + vtimeSlice.
 * ```
 *  
 * We can now see that finding a thread with a low virtual deadline is equivalent to finding a thread with a low lag, since each thread will expect to run for the same amount of virtual time, as described above. Therefore, there is no need to actually determine the lag, instead we can just use the virtual deadline as a proxy for lag, which is far simpler.
 * 
 * ## Scheduling
 * 
 * The scheduler maintains a runqueue of all runnable threads, sorted by their virtual deadlines. When a threads time slice expires, or a thread blocks or exits, the scheduler selects the thread with the earliest virtual deadline from the runqueue to run next. This ensures that the thread with the lowest lag is always selected to run next, resulting in time slices being distributed in such a way to converge towards the ideal CPU time shares for each thread.
 * 
 * @see [Earliest Eligible Virtual Deadline First](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=805acf7726282721504c8f00575d91ebfd750564) for
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
typedef uint64_t vclock_t;

/**
 * @brief Per-thread scheduler context.
 * @struct sched_ctx_t
 */
typedef struct sched_ctx
{
    rbnode_t node;
    uint64_t weight;    ///< The weight of the thread, derived from its process priority.
    vclock_t vruntime;  ///< Virtual runtime (how much time the thread has run in virtual time).
    vclock_t vdeadline; ///< Virtual deadline (when the thread is expected to finish in virtual time).
    clock_t lastUpdate; ///< Uptime when the thread last started running.
    clock_t timeSlice;  ///< The max duration of the current time slice.
} sched_ctx_t;

/**
 * @brief Per-CPU scheduler.
 * @struct sched_t
 */
typedef struct sched
{
    rbtree_t runqueue;    ///< Contains all runnable threads, sorted by virtual deadline.
    vclock_t minVruntime; ///< The minimum virtual runtime of all threads ever scheduled.
    uint64_t totalWeight; ///< The total weight of all threads in the runqueue.
    lock_t lock;
    thread_t* idleThread;
    thread_t* runThread;
} sched_t;

/**
 * @brief Initialize the scheduler context for a thread.
 *
 * @param ctx The scheduler context to initialize.
 */
void sched_ctx_init(sched_ctx_t* ctx);

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
 * @brief Submits a thread to be scheduled on a target CPU.
 *
 * If the target is not the current CPU, or the target is the current CPU and the current CPU is not in an interrupt, an inter-processor interrupt (IPI) will be sent to wake up the target CPU.
 * 
 * @warning This will take ownership of the thread. Therefore, the caller should never access the thread after calling
 * this function as, if preemption occurs, it could be freed by the time the function returns.
 *
 * @param thread The thread to submit.
 * @param target The target cpu to submit the thread to, or `NULL` for the current cpu.
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
NORETURN extern void sched_idle_loop(void);

/** @} */
