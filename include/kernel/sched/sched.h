#pragma once

#include <kernel/defs.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/rbtree.h>

#include <sys/list.h>
#include <sys/proc.h>

typedef struct process process_t;
typedef struct thread thread_t;

typedef struct sched sched_t;

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
 * The algorithm is relatively simple conceptually, but it is also very fragile, even small mistakes can easily result
 * in highly unfair scheduling. Therefore, if you find issues or bugs with the scheduler, please open an issue in the
 * GitHub repository.
 *
 * Included below is an overview of how the scheduler works and the relevant concepts. If you are unfamiliar with
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
 * Using this definition of virtual time, we can determine the amount of virtual time \f$v\f$ that has passed between
 * two points in real time \f$t_1\f$ and \f$t_2\f$ as
 *
 * \begin{equation*}
 * v = \frac{t_2 - t_1}{\sum_{i \in A(t_2)} w_i}
 * \end{equation*}
 *
 * under the assumption that \f$A(t_1) = A(t_2)\f$, i.e. the set of active threads has not changed between \f$t_1\f$ and
 * \f$t_2\f$.
 *
 * Note how the denominator containing the \f$\sum\f$ symbol evaluates to the sum of all weights \f$w_i\f$ for each
 * active thread \f$i\f$ in \f$A\f$ at \f$t_2\f$, i.e. the total weight of the scheduler cached in `sched->totalWeight`.
 * In pseudocode, this can be expressed as
 *
 * ```
 * vclock_t vtime = (sys_time_uptime() - oldTime) / sched->totalWeight;
 * ```
 *
 * Additionally, the amount of real time a thread should receive \f$r_i\f$ in a given duration of virtual time \f$v\f$
 * can be calculated as
 *
 * \begin{equation*}
 * r_i = v \cdot w_i.
 * \end{equation*}
 *
 * In practice, all we are doing is taking a duration of real time equal to the total weight of all active threads, and
 * saying that each thread ought to receive a portion of that time equal to its weight. Virtual time is just a trick to
 * simplify the math.
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
 * The lag \f$l_i\f$ of a thread \f$i\f$ is defined as
 *
 * \begin{equation*}
 * l_i = r_{i}^{should} - r_{i}^{actual}
 * \end{equation*}
 *
 * where \f$r_{i}^{should}\f$ is the amount of real time thread \f$i\f$ should have received and \f$r_{i}^{actual}\f$ is
 * the amount of real time thread \f$i\f$ has actually received.
 *
 * If we assume that all real time is used by the threads in \f$A(t)\f$, which is always true, and that all real time is
 * allocated among these threads, we can see that
 *
 * \begin{equation*}
 * \sum_{i \in A(t)} \left(r_{i}^{should} - r_{i}^{actual}\right) = 0
 * \end{equation*}
 *
 * or in other words, the sum of all lag values across all active threads is always zero.
 *
 * As an example, let's say we have three threads A, B and C with equal weights. To start with each thread is supposed
 * to have run for 0ms, and has actually run for 0ms, so their lag values are:
 *
 * <div align="center">
 * Thread | Lag (ms)
 * -------|-------
 *    A   |   0
 *    B   |   0
 *    C   |   0
 * </div>
 *
 * Now, let's say we give a 30ms (in real time) time slice to thread A, while threads B and C do not run at all. After
 * this, the lag values would be:
 *
 * <div align="center">
 * Thread | Lag (ms)
 * -------|-------
 *    A   |  -20
 *    B   |   10
 *    C   |   10
 * </div>
 *
 * What just happened is that each thread should have received one third of the real time (since they are all of equal
 * weight such that each of their weights is 1/3 of the total weight) which is 10ms. Therefore, since thread A actually
 * received 30ms of real time, it has run for 20ms more than it should have. Meanwhile, threads B and C have not
 * received any real time at all, so they are "owed" 10ms each.
 *
 * Additionally notice that \f$0 + 0 + 0 = 0\f$ and \f$-20 + 10 + 10 = 0\f$, i.e. the sum of all lag values is still
 * zero.
 *
 * Finally, this lets us determine the eligibility of a thread. A thread is considered eligible if, and only if, its lag
 * is greater than or equal to zero. In the above example threads B and C are eligible to run, while thread A is not.
 * Notice that due to the sum of all lag values being zero, this means that there will always be at least one eligible
 * thread as long as there is at least one active thread, since if there is a thread with negative lag then there must
 * be at least one thread with positive lag to balance it out.
 *
 * @note Fairness is achieved over some long period of time over which the proportion of real time each thread has
 * received will converge to the share it ought to receive. It does not guarantee that each individual time slice is
 * exactly correct, hence it's acceptable for thread A to receive 30ms of real time in the above example.
 *
 * @see [EEVDF](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=805acf7726282721504c8f00575d91ebfd750564)
 * pages 3-5.
 * @see [Completing the EEVDF Scheduler](https://lwn.net/Articles/969062/).
 *
 * ## Eligible Time
 *
 * In most cases, it's undesirable to track lag directly as it would require updating the lag of all threads whenever
 * the scheduler's virtual time is updated, which would violate the desired \f$O(\log n)\f$ complexity of the scheduler.
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
 * a thread should have received its due share of CPU time, rounded to some quantum. The scheduler always selects the
 * eligible thread with the earliest virtual deadline to run next.
 *
 * We can calculate the virtual deadline \f$v_{di}\f$ of a thread as
 *
 * \begin{equation*}
 * v_{di} = v_{ei} + \frac{Q}{w_i}
 * \end{equation*}
 *
 * where \f$Q\f$ is a constant time slice defined by the scheduler, in our case `CONFIG_TIME_SLICE`.
 *
 * @see [EEVDF](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=805acf7726282721504c8f00575d91ebfd750564)
 * page 3.
 *
 * ## Rounding Errors
 *
 * Before describing the implementation, it is important to note that due to the nature of integer division, rounding
 * errors are inevitable when calculating virtual time and lag.
 *
 * For example, when computing \f$10/3 = 3.333...\f$ we instead get \f$3\f$, losing the fractional part. Over time,
 * these small errors can accumulate and lead to unfair scheduling.
 *
 * It might be tempting to use floating point to mitigate these errors, however using floating point in a kernel is
 * generally considered very bad practice, only user space should, ideally, be using floating point.
 *
 * Instead, we use a simple technique to mitigate the impact of rounding errors. We represent virtual time and lag using
 * 128-bit fixed-point arithmetic, where the lower 63 bits represent the fractional part.
 *
 * There were two reasons for the decision to use 128 bits over 64 bits despite the performance cost. First, it means
 * that even the maximum possible value of uptime, stored using 64 bits, can still be represented in the fixed-point
 * format without overflowing the integer part, meaning we don't need to worry about overflow at all.
 *
 * Second, testing shows that lag appears to accumulate an error of about \f$10^3\f$ to \f$10^4\f$ in the fractional
 * part every second under heavy load, meaning that using 64 bits and a fixed point offset of 20 bits, would result in
 * an error of approximately 1 nanosecond per minute, considering that the testing was not particularly rigorous, it
 * might be significantly worse in practice. Note that at most every division can create an error equal to the divider
 * minus one in the fractional part.
 *
 * If we instead use 128 bits with a fixed point offset of 63 bits, the same error of \f$10^4\f$ in the fractional part
 * results in an error of approximately \f$1.7 \cdot 10^{-9}\f$ nanoseconds per year, which is obviously negligible
 * even if the actual error is in reality several orders of magnitude worse.
 *
 * For comparisons between `vclock_t` values, we consider two values equal if the difference between their whole parts
 * is less than or equal to `VCLOCK_EPSILON`.
 *
 * Some might feel concerned about the performance impact of using 128-bit arithmetic. However, consider that by using
 * 128-bit arithmetic, we no longer need any other means of reducing rounding errors. We dont need to worry about
 * remainders from divisions, dividing to the nearest integer instead of rounding down, etc. This not only simplifies
 * the code drastically, making it more approachable, but it also means that, in practice, the performance impact is
 * negligible. Its a very simple brute force solution, but simple does not mean bad.
 *
 * @see [Fixed Point Arithmetic](https://en.wikipedia.org/wiki/Fixed-point_arithmetic)
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
 * thread.
 *
 * ## Idle Thread
 *
 * The idle thread is a special thread that is not considered active (not stored in the runqueue) and simply runs an
 * infinite loop that halts the CPU while waiting for an interrupt signaling that a non-idle thread is available to run.
 * Each CPU has its own idle thread.
 *
 * ## Load Balancing
 *
 * Each CPU has its own scheduler and associated runqueue, as such we need to balance the load between each CPU, ideally
 * without causing too many cache misses. Meaning we want to keep threads which have recently run on a CPU on the same
 * CPU when possible. As such, we define a thread to be "cache-cold" on a CPU if the time since it last ran on that CPU
 * is greater than `CONFIG_CACHE_HOT_THRESHOLD`, otherwise its considered "cache-hot".
 *
 * We use two mechanisms to balance the load between CPUs, one push mechanism and one pull mechanism.
 *
 * The push mechanism, also called work stealing, is used when a thread is submitted to the scheduler, as in it was
 * created or unblocked. In this case, if the thread is cache-cold then the thread will be added to the runqueue of the
 * CPU with the lowest weight. Otherwise, it will be added to the runqueue of the CPU it last ran on.
 *
 * The pull mechanism is used when a CPU is about to become idle. The CPU will find the CPU with the highest weight and
 * steal the first cache-cold thread from its runqueue. If no cache-cold threads are found, it will simply run the idle
 * thread.
 *
 * @note The reason we want to avoid a global runqueue is to avoid lock contention. Even a small amount of lock
 * contention in the scheduler will quickly degrade performance, as such it is only allowed to lock a single CPU's
 * scheduler at a time. This does cause race conditions while pulling or pushing threads, but the worst case scenario is
 * imperfect load balancing, which is acceptable.
 *
 * ## Testing
 *
 * The scheduler is tested using a combination of asserts and tests that are enabled in debug builds (`NDEBUG` not
 * defined). These tests verify that the runqueue is sorted, that the lag does sum to zero (within a margin from
 * rounding errors), and other invariants of the scheduler.
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
typedef int128_t vclock_t;

/**
 * @brief Lag type.
 * @struct lag_t
 */
typedef int128_t lag_t;

/**
 * @brief The bits used for the fractional part of a virtual clock or lag value.
 *
 * One sign bit, 64 integer bits, 63 fractional bits.
 */
#define SCHED_FIXED_POINT (63LL)

/**
 * @brief Fixed-point zero.
 */
#define SCHED_FIXED_ZERO ((int128_t)0)

/**
 * @brief The minimum difference between two virtual clock or lag values to consider then unequal.
 */
#define SCHED_EPSILON (10LL)

/**
 * @brief Convert a regular integer to fixed-point representation.
 */
#define SCHED_FIXED_TO(x) (((int128_t)(x)) << SCHED_FIXED_POINT)

/**
 * @brief Convert a fixed-point value to a regular integer.
 */
#define SCHED_FIXED_FROM(x) ((int64_t)(((int128_t)(x)) >> SCHED_FIXED_POINT))

/**
 * @brief The maximum weight a thread can have.
 */
#define SCHED_WEIGHT_MAX (PRIORITY_MAX + SCHED_WEIGHT_BASE)

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
    clock_t stop;          ///< The real time when the thread previously stopped executing.
    cpu_t* lastCpu;        ///< The last CPU the thread was scheduled on, it stoped running at `stop` time.
} sched_client_t;

/**
 * @brief Per-CPU scheduler.
 * @struct sched_t
 *
 * Stored in a CPU's `sched` member.
 */
typedef struct sched
{
    _Atomic(int64_t) totalWeight; ///< The total weight of all threads in the runqueue, not protected by the lock.
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
 * 
 * @todo Currently not implemented as we cant really yield as that would break fairness. Maybe we could pretend to leave and re-enter?
 */
void sched_yield(void);

/**
 * @brief Submits a thread to the scheduler.
 *
 * If the thread has previously ran within `CONFIG_CACHE_HOT_THRESHOLD` nanoseconds, it will be submitted to the same
 * CPU it last ran on, otherwise it will be submitted to the least loaded CPU.
 *
 * @param thread The thread to submit.
 */
void sched_submit(thread_t* thread);

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
