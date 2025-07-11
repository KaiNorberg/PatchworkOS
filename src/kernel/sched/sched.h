#pragma once

#include "defs.h"
#include "sched/wait.h"
#include "sync/lock.h"

#include <sys/list.h>
#include <sys/proc.h>

typedef struct process process_t;
typedef struct thread thread_t;

/**
 * @brief The Scheduler.
 * @defgroup kernel_sched sched
 * @ingroup kernel
 *
 * The scheduler used in Patchwork is loosely based of the Linux O(1) scheduler, so knowing how that scheduler works
 * could be useful, here is an article about it https://litux.nl/mirror/kerneldevelopment/0672327201/ch04lev1sec2.html.
 */

/**
 * @brief Scheduling queues structure.
 * @ingroup kernel_sched
 * @struct sched_queues_t
 *
 * The `sched_queues_t` structure represents a set of scheduling queues, with one queue for each priority level and a
 * bitmap for faster lookups.
 *
 */
typedef struct
{
    /**
     * @brief The total number of threads in all lists.
     */
    uint64_t length;
    /**
     * @brief A bitmap indicating which priority lists have threads in them.
     */
    uint64_t bitmap;
    /**
     * @brief An array of lists that store threads, one for each priority.
     */
    list_t lists[PRIORITY_MAX];
} sched_queues_t;

/**
 * @brief Per-thread scheduling context.
 * @ingroup kernel_sched
 * @struct sched_thread_ctx_t
 *
 * The `sched_thread_ctx_t` structure stores scheduling context for each thread.
 *
 */
typedef struct
{
    /**
     * @brief The length of the threads time slice, used to determine its deadline when its scheduled.
     */
    clock_t timeSlice;
    /**
     * @brief The time when the time slice will actually expire, only valid while its running.
     */
    clock_t deadline;
    /**
     * @brief The actual priority of the thread.
     *
     * Based of the processes priority, but with some dynamic changes determined by if the thread is IO bound or CPU
     * bound.
     *
     */
    priority_t actualPriority;
    /**
     * @brief The amount of time within the last `CONFIG_MAX_RECENT_BLOCK_TIME` nanoseconds that the thread was
     * blocking.
     *
     * Used to determine its actual priority.
     *
     */
    clock_t recentBlockTime;
    /**
     * @brief The previous time when the `recentBlockTime` member was updated.
     */
    clock_t prevBlockCheck;
} sched_thread_ctx_t;

/**
 * @brief Per-CPU scheduling context.
 * @ingroup kernel_sched
 * @struct sched_cpu_ctx_t
 *
 * The `sched_cpu_ctx_t` structure holds the scheduling context for a each CPU.
 *
 */
typedef struct
{
    /**
     * @brief Array storing both queues.
     *
     * Should never be accessed always use the pointer `active` and `expired` as those always point to these queues.
     *
     */
    sched_queues_t queues[2];
    /**
     * @brief Pointer to the currently active queue.
     */
    sched_queues_t* active;
    /**
     * @brief Pointer to the currently expired queue.
     */
    sched_queues_t* expired;

    /**
     * @brief The currently running thread.
     *
     * Accessing the run thread can be a bit weird; if the run thread is accessed by the currently running thread,
     * then there is no need for a lock as it will always see the same value, itself. However, if it is accessed
     * from another CPU, then the lock is needed.
     */
    thread_t* runThread;
    /**
     * @brief The thread that runs when the owner CPU is idling.
     *
     * This thread never changes after boot, so no need for a lock.
     */
    thread_t* idleThread;
    /**
     * @brief The lock that protects this context, except the `zombieThreads` list.
     */
    lock_t lock;
    /**
     * @brief Stores threads after they have been killed.
     *
     * This is used to prevent the kernel from using the kernel stack of a freed thread. Only accessed by the owner CPU,
     * so no need for a lock.
     */
    list_t zombieThreads;
} sched_cpu_ctx_t;

/**
 * @brief Initializes a thread's scheduling context.
 * @ingroup kernel_sched
 *
 * The `sched_thread_ctx_init()` function initializes the `sched_thread_ctx_t` structure for a new thread.
 *
 * @param ctx The `sched_thread_ctx_t` structure to initialize.
 */
void sched_thread_ctx_init(sched_thread_ctx_t* ctx);

/**
 * @brief Initializes a CPU's scheduling context.
 * @ingroup kernel_sched
 *
 * The `sched_cpu_ctx_init()` function initializes the `sched_cpu_ctx_t` structure for a CPU.
 *
 * @param ctx The `sched_cpu_ctx_t` structure to initialize.
 * @param cpu The `cpu_t` structure associated with this scheduling context.
 */
void sched_cpu_ctx_init(sched_cpu_ctx_t* ctx, cpu_t* cpu);

/**
 * @brief The idle loop for a CPU.
 * @ingroup kernel_sched
 *
 * The `sched_idle_loop()` function is the main loop where idle threads execute.
 *
 */
NORETURN extern void sched_idle_loop(void);

/**
 * @brief Wrapper around `sched_schedule()`.
 * @ingroup kernel_sched
 *
 * The `sched_invoke()` function constructs a trap frame using current CPU state and then calls
 * `sched_schedule()`. This is typically used for voluntary context switches, such as when blocking.
 *
 */
extern void sched_invoke(void);

/**
 * @brief Initializes the scheduler.
 * @ingroup kernel_sched
 *
 * The `sched_init()` function performs global initialization for the scheduler, for example spawning the boot thread.
 *
 */
void sched_init(void);

/**
 * @brief Specify that the boot thread is no longer needed.
 * @ingroup kernel_sched
 *
 * The `sched_done_with_boot_thread()` function is called to tell the scheduler that the kernel has finished booting and
 * that the boot thread is no longer needed, instead of just discarding it, the boot thread becomes the idle thread of
 * the bootstrap cpu.
 *
 */
NORETURN void sched_done_with_boot_thread(void);

/**
 * @brief Puts the current thread to sleep.
 * @ingroup kernel_sched
 *
 * The `sched_sleep()` function causes the currently running thread to block, for a specified length of time.
 *
 * @param timeout The maximum time to sleep. If `CLOCKS_NEVER`, it sleeps forever.
 * @return A `wait_result_t` indicating why the thread woke up (e.g., timeout, signal).
 */
wait_result_t sched_sleep(clock_t timeout);

/**
 * @brief Checks if the current CPU is idle.
 * @ingroup kernel_sched
 *
 * The `sched_is_idle()` function returns if the current CPU is currently executing its idle thread.
 *
 * @return `true` if the CPU is idle, `false` otherwise.
 */
bool sched_is_idle(void);

/**
 * @brief Retrieves the currently running thread.
 * @ingroup kernel_sched
 *
 * The `sched_thread()` function returns the currently running thread.
 *
 * @return The currently running thread.
 */
thread_t* sched_thread(void);

/**
 * @brief Retrieves the process of the currently running thread.
 * @ingroup kernel_sched
 *
 * The `sched_process()` function returns the process of the currently running thread.
 *
 * @return The process of the currently running thread.
 */
process_t* sched_process(void);

/**
 * @brief Exits the current process.
 * @ingroup kernel_sched
 *
 * The `sched_process_exit()` function terminates the currently executing process and all its threads. Note that this
 * does not actually schedule and the thread will only actually die, when it is scheduled.
 *
 * @param status The exit status of the process. (Not implemented, i will get around to it... maybe)
 */
void sched_process_exit(uint64_t status);

/**
 * @brief Exits the current thread.
 * @ingroup kernel_sched
 *
 * The `sched_thread_exit()` function terminates the currently executing thread. Note that this does not actually
 * schedule and the thread will only actually die, when it is scheduled.
 *
 */
void sched_thread_exit(void);

/**
 * @brief Yields the CPU to another thread.
 * @ingroup kernel_sched
 *
 * The `sched_yield()` function voluntarily relinquishes the currently running threads time slice.
 *
 */
void sched_yield(void);

/**
 * @brief Pushes a thread onto a scheduling queue.
 * @ingroup kernel_sched
 *
 * The `sched_push()` function adds a thread to the appropriate scheduling queue, making it runnable.
 *
 * @param thread The thread to be pushed.
 * @param parent The parent thread (can be `NULL`).
 * @param target The target cpu that the thread should run on (can be `NULL` to specify the currently running cpu).
 */
void sched_push(thread_t* thread, thread_t* parent, cpu_t* target);

/**
 * @brief Performs the core scheduling logic.
 * @ingroup kernel_sched
 *
 * The `sched_schedule()` function is the heart of the scheduler, responsible for selecting the next
 * thread to run and performing the context switch.
 *
 * @param trapFrame The current trap frame.
 * @param self The currently running cpu
 * @return `true` if a context switch occurred, `false` otherwise.
 */
bool sched_schedule(trap_frame_t* trapFrame, cpu_t* self);
