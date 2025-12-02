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
 * The scheduler is responsible for allocating CPU time to threads, deciding which thread to run next and for how long,
 * it does this in such a way to create the illusion that multiple threads are running simultaneously on a single CPU.
 * Consider that a video is in reality just a series of still images, rapidly displayed one after the other. The
 * scheduler works in the same way, rapidly switching between threads to give the illusion of simultaneous execution.
 *
 * PatchworkOS uses the Earliest Eligible Virtual Deadline First (EEVDF) algorithm for its scheduler. EEVDF attempts to
 * give each thread a fair share of the CPU based on its weight by introducing the concepts of virtual time and virtual
 * deadlines. This is in contrast to more common algorithms that use fixed time slices or priority queues.
 *
 * The algorithm is relatively simple conceptually, but it is very "finicky" to implement correctly, even small mistakes
 * can easily result in highly unfair scheduling. Therefore, if you find issues or bugs with the scheduler, please open
 * an issue in the GitHub repository.
 *
 * Included below is a basic overview of how the scheduler works.
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
 * @see [EEVDF](https://www.cs.utexas.edu/~pingali/CS380C/eevdf.pdf) page 2.
 *
 * ## Virtual Time
 *
 * The EEVDF algorithm introduces the concept of "virtual time", this is the mechanism that tracks how much real CPU time
 * each thread ought to receive. Each scheduler maintains a "virtual clock" that runs at a rate inversely proportional
 * to the `totalWeight` of all active threads (all threads in the runqueue plus the currently running thread). So, if
 * the total weight is `10`, then each unit of virtual time corresponds to `10` units of real time.
 * 
 * Each thread should receive an amount of real CPU time equal to its weight for each virtual time unit that passes. For
 * example, if we have two threads A and B with weights `2` and `3` respectively, then for every `1` unit of virtual
 * time that passes, thread A should receive `2` units of real CPU time and thread B should receive `3` units of real
 * CPU time.
 * 
 * Using this definition of virtual time, we can see that to convert from real time to virtual time, we do
 * 
 * ```
 * vtime = time / totalWeight
 * ```
 * 
 * and the amount of real time a thread should receive based on its weight can be calculated as
 * 
 * ```
 * time = vtime * weight.
 * ```
 *
 * @note All variables storing virtual time values will be prefixed with 'v' and use the `vclock_t` type. Variables
 * storing real time values will use the `clock_t` type as normal. The `vclock_t` type is signed, as lag values can be
 * negative and due to that fact, virtual time can also be negative, as such virtual time can not be assumed to be
 * monotonically increasing.
 *
 * @see [EEVDF](https://www.cs.utexas.edu/~pingali/CS380C/eevdf.pdf) pages 8-9.
 *
 * ## Lag
 *
 * Now we can move on to the metrics used to select threads. There are, as the name "Earliest Eligible Virtual Deadline First"
 * suggests, two main concepts relevant to this process. Its "eligibility" and its "virtual deadline". We will
 * start with "eligibility", which is determined by the concept of "lag".
 *
 * Lag is defined as the difference between the amount of real CPU time a thread should have received and the amount of
 * real CPU time it has actually received.
 *
 * As an example, lets say we have three threads A, B and C with equal weights. To start with each thread is supposed to
 * have run for 0ms, and has actually run for 0ms:
 *
 * <div align="center">
 * Thread | Lag (ms)
 * -------|-------
 *    A   |   0
 *    B   |   0
 *    C   |   0
 * </div>
 *
 * Now, lets say we give a 30ms (in real time) time slice to thread A. The lag values will now be:
 *
 * <div align="center">
 * Thread | Lag (ms)
 * -------|-------
 *    A   |  -20
 *    B   |   10
 *    C   |   10
 * </div>
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
 * @note Fairness is achieved over some long period of time over which the proportion of CPU time each thread has
 * received will converge to the share it ought to receive. It does not guarantee that each individual time slice is exactly correct, hence its acceptable for thread A to receive 30ms of CPU time in the above example.
 *
 * @see [EEVDF](https://www.cs.utexas.edu/~pingali/CS380C/eevdf.pdf) pages 3-5.
 * @see [Completing the EEVDF Scheduler](https://lwn.net/Articles/969062/).
 *
 * ## Eligible Time
 * 
 * In most cases, its undesirable to track lag directly, partially due to rounding errors but primarily because it would require updating the lag of all threads whenever the scheduler's virtual time is updated, which would violate the `O(log n)` complexity of the scheduler.
 * 
 * Instead, EEVDF defines the concept of "eligible time" (`veligible`) as the virtual time at which a thread's lag becomes zero, which is equivalent to the virtual time at which the thread becomes eligible to run.
 * 
 * When a thread enters the scheduler for the first time, its `veligible` is set to the current virtual time (equivalent to a lag of 0). Then, when the thread is preempted, the amount of virtual time is has used will be added to its `veligible`.
 * 
 * When penalizing or compensating a thread the virtual clock must remain consistent. To achieve this, we adjust the
 * scheduler's virtual clock when a thread enters the scheduler by subtracting the thread's lag converted to virtual
 * time. Conversely, when a thread leaves the scheduler, we adjust the scheduler's virtual clock by adding the thread's
 * lag converted to virtual time. The proof for this is outside the scope of this documentation, but it can be found in
 * the EEVDF paper.
 *
 * @note This implementation follows "Strategy 1" as described in the EEVDF paper, but we also apply a maximum ca0p
 * (`CONFIG_MAX_LAG`) to the lag value to prevent a thread from becoming "entitled" to a theoretically infinite amount
 * of CPU time by just sleeping for a long time.
 *
 * @see [EEVDF](https://www.cs.utexas.edu/~pingali/CS380C/eevdf.pdf) pages 10-12 and 14.
 *
 * ## Virtual Deadlines
 *
 * We can now move on to the other part of the name, "virtual deadline". The goal of the scheduler is to always run the
 * eligible thread with the earliest virtual deadline, we now know what makes a thread eligible. So, what is a virtual
 * deadline?
 *
 * A virtual deadline is defined as the earliest time at which a thread should have received its due share of CPU time.
 * Which is determined as the sum of the virtual time at which the thread becomes eligible (`veligible`) and the
 * amount of virtual time corresponding to the thread's next time slice.
 *
 * @see [EEVDF](https://www.cs.utexas.edu/~pingali/CS380C/eevdf.pdf) page 3.
 *
 * ## Scheduling
 *
 * With the central concepts introduced, we can now describe how the scheduler works. As mentioned, the goal is to
 * always run the eligible thread with the earliest virtual deadline. To achieve this, each scheduler maintains a
 * runqueue in the form of a Red-Black tree sorted by each thread's virtual deadline.
 *
 * To select the next thread to run, we find the first eligible thread in the runqueue and switch to it. If no eligible
 * thread is found, we switch to the idle thread. This process is optimized by storing the minimum eligible time of each
 * subtree in each node of the runqueue, allowing us to skip entire subtrees that do not contain any eligible threads.
 *
 * The idle thread is a special thread that is not considered active and simply runs an infinite loop that halts the CPU
 * while waiting for an interrupt signaling work to do.
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
 * Testing is done via asserts and additional debug checks in debug builds. For example, to validate the ordering of the
 * runqueue.
 *
 * ## References
 *
 * References were accessed on 2025-12-01.
 *
 * [Ion Stoica, Hussein Abdel-Wahab, "Earliest Eligible Virtual Deadline First", Department of Computer Science, Old Dominion University, 1996.](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=805acf7726282721504c8f00575d91ebfd750564)
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
    __int128_t num; ///< The numerator.
    __int128_t den; ///< The denominator.
} vclock_t;

/**
 * @brief Virtual clock representing zero time.
 */
#define VCLOCK_ZERO ((vclock_t){ .num = 0, .den = 1 })

/**
 * @brief Virtual clock representing the default time slice.
 */
#define VCLOCK_TIME_SLICE ((vclock_t){ .num = CONFIG_TIME_SLICE, .den = 1 })

/**
 * @brief Lag type.
 * @struct lag_t
 */
typedef vclock_t lag_t;

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
    vclock_t veligible;        ///< The virtual time at which the thread becomes eligible to run (lag >= 0).
    vclock_t vminEligible; ///< The minimum virtual eligible time of the subtree in the runqueue.
    vclock_t vleave;           ///< The virtual time when the thread left the scheduler (blocked), or `VCLOCKS_NEVER`.
    clock_t start;             ///< The real time when the thread started executing its current time slice.
    uint64_t resetCounter;      ///< The scheduler reset counter when the thread last left the scheduler.
} sched_client_t;

/**
 * @brief Per-CPU scheduler.
 * @struct sched_t
 *
 * Stored in a CPU's `sched` member.
 */
typedef struct sched
{
    int64_t totalWeight;  ///< The total weight of all threads in the runqueue.
    rbtree_t runqueue;    ///< Contains all runnable threads, including the currently running thread, sorted by vdeadline.
    vclock_t vtime;       ///< The current virtual time of the CPU.
    clock_t lastUpdate;   ///< The real time when the last vtime update occurred.
    uint64_t resetCounter;  ///< Counter incremented each time the vtime is reset.
    lock_t lock;          ///< The lock protecting the scheduler.
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
void sched_enter(thread_t* thread, cpu_t* target);

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
