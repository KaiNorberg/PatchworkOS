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
 * The scheduler is responsible for allocating CPU time to threads, it does this in such a way to create the illusion
 * that multiple threads are running simultaneously on a single CPU. Consider that a video is in reality just a series
 * of still images, rapidly displayed one after the other. The scheduler works in the same way, rapidly switching
 * between threads to give the illusion of simultaneous execution.
 *
 * PatchworkOS uses the Earliest Eligible Virtual Deadline First (EEVDF) algorithm for its scheduler, which is a
 * proportional share scheduling algorithm that aims to fairly distribute CPU time among threads based on their weights.
 * This is in contrast to more traditional scheduling algorithms like round-robin or priority queues.
 *
 * The algorithm is relatively simple conceptually, but it is also very fragile, even small mistakes can easily result in highly unfair scheduling. Therefore, if you find issues or bugs with the scheduler, please open an issue in the GitHub repository.
 *
 * Included below is a overview of how the scheduler works and the relevant concepts. If you are unfamiliar with
 * mathematical notation, don't worry, we will explain everything in plain English as well.
 *
 * ## Weight and Priority
 *
 * First, we need to assign each thread a "weight", denoted as \f$w_i\f$ where \f$i\f$ uniquely identifies the thread
 * and, for completeness, let's define the set \f$A(t)\f$ which contains all active threads at real time \f$t\f$. To
 * simplify, for thread \f$i\f$, its weight is \f$w_i\f$.
 *
 * A thread's weight is calculated as the sum of the process's priority and a constant `SCHED_WEIGHT_BASE`, the constant
 * is needed to ensure that all threads have a weight greater than zero, as that would result in division by zero errors
 * later on.
 *
 * The weight is what determines the share of CPU time a thread ought to receive, with a higher weight receiving a
 * larger share. Specifically, the fraction of CPU time a thread receives is proportional to its weight relative to the
 * total weight of all active threads. This is implemented using "virtual time", as described below.
 *
 * @see [EEVDF](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=805acf7726282721504c8f00575d91ebfd750564)
 * page 2.
 *
 * ## Virtual Time
 *
 * The first relevant concept that the EEVDF algorithm introduces is "virtual time". Each scheduler maintains a "virtual
 * clock" that runs at a rate inversely proportional to the total weight of all active threads (all threads in the
 * runqueue). So, if the total weight is \f$10\f$ then each unit of virtual time corresponds to \f$10\f$ units of real
 * CPU time.
 *
 * Each thread should receive an amount of real time equal to its weight for each virtual time unit that passes. For
 * example, if we have two threads, A and B, with weights \f$2\f$ and \f$3\f$ respectively, then for every \f$1\f$ unit
 * of virtual time, thread A should receive \f$2\f$ units of real time and thread B should receive \f$3\f$ units of real
 * time. Which is equivalent to saying that for every \f$5\f$ units of real time, thread A should receive \f$2\f$ units
 * of real time and thread B should receive \f$3\f$ units of real time.
 *
 * Using this definition of virtual time, we can see that to convert some change in real time \f$\Delta t\f$ to a change
 * in virtual time \f$\Delta v\f$, we say
 *
 * \begin{equation*}
 * \Delta v = \frac{\Delta t}{\sum_{i \in A(t)} w_i}
 * \end{equation*}
 *
 * Note how the denominator containing the \f$\sum\f$ symbol simply means the sum of all weights \f$w_i\f$ for each
 * active thread \f$i\f$ at real time \f$t\f$, i.e. the total weight of the scheduler cached in `sched->totalWeight`. In
 * pseudocode, this would be:
 *
 * ```
 * vclock_t vtime = sys_time_uptime() / sched->totalWeight;
 * ```
 *
 * Additionally, the amount of real time a thread should receive \f$r_i\f$ in a given duration of virtual time \f$\Delta
 * v\f$ can be calculated as
 *
 * \begin{equation*}
 * r_i = \Delta v \cdot w_i.
 * \end{equation*}
 *
 * In practice, all we are doing is taking a duration of real time equal to the total weight of all active threads, and
 * saying that each thread ought to receive a portion of that time equal to its weight.
 *
 * @note All variables storing virtual time values will be prefixed with 'v' and use the `vclock_t` type. Variables
 * storing real time values will use the `clock_t` type as normal.
 *
 * @see [EEVDF](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=805acf7726282721504c8f00575d91ebfd750564)
 * pages 8-9.
 *
 * ## Lag
 *
 * Now we can move on to the metrics used to select threads. There are, as the name "Earliest Eligible Virtual Deadline
 * First" suggests, two main concepts relevant to this process. Its "eligibility" and its "virtual deadline". We will
 * start with "eligibility", which is determined by the concept of "lag".
 *
 * Lag is defined as the difference between the amount of real time a thread should have received and the amount of real
 * time it has actually received.
 *
 * As an example, lets say we have three threads X, Y and Z with equal weights. To start with each thread is supposed to
 * have run for 0ms, and has actually run for 0ms, so their lag values are:
 *
 * <div align="center">
 * Thread | Lag (ms)
 * -------|-------
 *    X   |   0
 *    Y   |   0
 *    Z   |   0
 * </div>
 *
 * Now, lets say we give a 30ms (in real time) time slice to thread X, while threads Y and Z do not run at all. After
 * this, the lag values would be:
 *
 * <div align="center">
 * Thread | Lag (ms)
 * -------|-------
 *    X   |  -20
 *    Y   |   10
 *    Z   |   10
 * </div>
 *
 * What just happened is that each thread should have received one third of the real time (since they are all of equal
 * weight such that each of their weights is 1/3 of the total weight) which is 10ms. Therefore, since thread X actually
 * received 30ms of real time, it has run for 20ms more than it should have. Meanwhile, threads Y and Z have not
 * received any real time at all, so they are "owed" 10ms each.
 *
 * One important property of lag is that the sum of all lag values across all active threads is always zero. In the
 * above examples, we can see that \f$0 + 0 + 0 = 0\f$ and \f$-20 + 10 + 10 = 0\f$.
 *
 * Finally, this lets us determine the eligibility of a thread. A thread is considered eligible if, and only if, its lag
 * is greater than or equal to zero. In the above example threads Y and Z are eligible to run, while thread X is not.
 * Notice that due to the sum of all lag values being zero, this means that there will always be at least one eligible
 * thread as long as there is at least one active thread, since if there is a thread with negative lag then there must
 * be at least one thread with positive lag to balance it out.
 *
 * @note Fairness is achieved over some long period of time over which the proportion of real time each thread has
 * received will converge to the share it ought to receive. It does not guarantee that each individual time slice is
 * exactly correct, hence its acceptable for thread X to receive 30ms of real time in the above example.
 *
 * @see [EEVDF](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=805acf7726282721504c8f00575d91ebfd750564)
 * pages 3-5.
 * @see [Completing the EEVDF Scheduler](https://lwn.net/Articles/969062/).
 *
 * ## Eligible Time
 *
 * In most cases, its undesirable to track lag directly as it would require updating the lag of all threads whenever the
 * scheduler's virtual time is updated, which would violate the desired \f$O(\log n)\f$ complexity of the scheduler.
 *
 * Instead, EEVDF defines the concept of "eligible time" as the virtual time at which a thread's lag
 * becomes zero, which is equivalent to the virtual time at which the thread becomes eligible to run.
 *
 * When a thread enters the scheduler for the first time, its eligible time \f$v_{ei}\f$ is the current virtual time of
 * the scheduler, which is equivalent to a lag of \f$0\f$. Whenever the thread runs, its eligible time is advanced by
 * the amount of virtual time corresponding to the real time it has used. This can be calculated as
 *
 * \begin{equation*}
 * v_{ei} = v_{ei} + \frac{t_{used}}{w_i}
 * \end{equation*}
 *
 * where \f$t_{used}\f$ is the amount of real time the thread has used, and \f$w_i\f$ is the thread's weight.
 *
 * @see [EEVDF](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=805acf7726282721504c8f00575d91ebfd750564)
 * pages 10-12 and 14.
 *
 * ## Virtual Deadlines
 *
 * We can now move on to the other part of the name, "virtual deadline", which is defined as the earliest time at which
 * a thread should have received its due share of CPU time. The scheduler always selects the eligible thread with the
 * earliest virtual deadline to run next.
 *
 * We can calculate the virtual deadline \f$v_{di}\f$ of a thread as
 *
 * \begin{equation*}
 * v_{di} = v_{ei} + \frac{Q}{w_i}
 * \end{equation*}
 *
 * where \f$Q\f$ is a constant time slice defined by the scheduler, in our case `CONFIG_TIME_SLICE`, however
 * `VCLOCK_TIME_SLICE` is provided for convenience as it is already converted to the `vclock_t` type.
 *
 * @see [EEVDF](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=805acf7726282721504c8f00575d91ebfd750564)
 * page 3.
 *
 * ## Scheduling
 *
 * With the central concepts introduced, we can now describe how the scheduler works. As mentioned, the goal is to
 * always run the eligible thread with the earliest virtual deadline. To achieve this, each scheduler maintains a
 * runqueue in the form of a Red-Black tree sorted by each thread's virtual deadline.
 *
 * To select the next thread to run, we find the first eligible thread in the runqueue and switch to it. If no eligible
 * thread is found (which means the runqueue is empty), we switch to the idle thread. This process is optimized by
 * storing the minimum eligible time of each subtree in each node of the runqueue, allowing us to skip entire subtrees
 * that do not contain any eligible threads.
 *
 * ## Preemption
 *
 * If, at any point in time, a thread with an earlier virtual deadline becomes available to run (for example, when a
 * thread is unblocked), the scheduler will preempt the currently running thread and switch to the newly available
 * thread. In practice, this improves responsiveness for interactive tasks like desktop applications.
 *
 * ## Idle Thread
 *
 * The idle thread is a special thread that is not considered active (not stored in the runqueue) and simply runs an
 * infinite loop that halts the CPU while waiting for an interrupt signaling that a non-idle thread is available to run.
 * Each CPU has its own idle thread.
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
 * ## References
 *
 * References were accessed on 2025-12-02.
 *
 * [Ion Stoica, Hussein Abdel-Wahab, "Earliest Eligible Virtual Deadline First", Old Dominion University,
 * 1996.](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=805acf7726282721504c8f00575d91ebfd750564)
 *
 * [Jonathan Corbet, "An EEVDF CPU scheduler for Linux", LWN.net, March 9, 2023.](https://lwn.net/Articles/925371/)
 *
 * [Jonathan Corbet, "Completing the EEVDF Scheduler", LWN.net, April 11, 2024.](https://lwn.net/Articles/969062/)
 *
 * @{
 */

/**
 * @brief Virtual clock type.
 * @typedef vclock_t
 */
typedef struct
{
    int64_t value;
    int64_t remainder;
} vclock_t;

#define VCLOCK_BASE ((int64_t)CLOCKS_PER_SEC)

#define VCLOCK_EPSILON ((int64_t)2)

/**
 * @brief Virtual clock representing zero time.
 */
#define VCLOCK_ZERO ((vclock_t){.value = 0, .remainder = 0})

/**
 * @brief Virtual clock representing the default time slice.
 */
#define VCLOCK_TIME_SLICE ((vclock_t){.value = CONFIG_TIME_SLICE, .remainder = 0})

/**
 * @brief Lag type.
 * @struct lag_t
 */
typedef vclock_t lag_t;

/**
 * @brief Base weight added to all threads.
 *
 * Used to prevent division by zero.
 */
#define SCHED_WEIGHT_BASE 1

/**
 * @brief Per-thread scheduler context.
 * @struct sched_client_t
 *
 * Stored in a thread's `sched` member.
 */
typedef struct sched_client
{
    rbnode_t node;  ///< The node in the scheduler's runqueue.
    int64_t weight; ///< The weight of the thread.
    /**
     * The earliest virtual time at which the thread ought to have received its due share of CPU time.
     */
    vclock_t vdeadline;
    vclock_t veligible;    ///< The virtual time at which the thread becomes eligible to run (lag >= 0).
    vclock_t vminEligible; ///< The minimum virtual eligible time of the subtree in the runqueue.
    clock_t start;         ///< The real time when the thread started executing its current time slice.
} sched_client_t;

/**
 * @brief Per-CPU scheduler.
 * @struct sched_t
 *
 * Stored in a CPU's `sched` member.
 */
typedef struct sched
{
    int64_t totalWeight; ///< The total weight of all threads in the runqueue.
    rbtree_t runqueue;  ///< Contains all runnable threads, including the currently running thread, sorted by vdeadline.
    vclock_t vtime;     ///< The current virtual time of the CPU.
    clock_t lastUpdate; ///< The real time when the last vtime update occurred.
    lock_t lock;        ///< The lock protecting the scheduler.
    thread_t* idleThread; ///< The idle thread for this CPU.
    thread_t* runThread;  ///< The currently running thread on this CPU.
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
