#pragma once

#include <kernel/defs.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>

#include <sys/list.h>
#include <sys/proc.h>

typedef struct process process_t;
typedef struct thread thread_t;

/**
 * @brief The Scheduler.
 * @defgroup kernel_sched The Scheduler
 * @ingroup kernel
 *
 * The scheduler used in Patchwork is loosely based of the Linux O(1) scheduler, so knowing how that scheduler works
 * could be useful, here is an article about it https://litux.nl/mirror/kerneldevelopment/0672327201/ch04lev1sec2.html.
 *
 * @{
 */

/**
 * @brief Scheduling queues structure.
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
     * @brief A bitmap indicating which of the lists have threads in them.
     */
    uint64_t bitmap;
    /**
     * @brief An array of lists that store threads, one for each priority, used in a round robin fashion.
     */
    list_t lists[PRIORITY_MAX];
} sched_queues_t;

/**
 * @brief Per-thread scheduling context.
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
     * @brief The time when the time slice will actually expire, only valid while the thread is running.
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
    cpu_t* owner; ///< The cpu that owns this scheduling context.
} sched_cpu_ctx_t;

/**
 * @brief Initializes a thread's scheduling context.
 *
 * @param ctx The `sched_thread_ctx_t` structure to initialize.
 */
void sched_thread_ctx_init(sched_thread_ctx_t* ctx);

/**
 * @brief Initializes a CPU's scheduling context.
 *
 * Will also register the schedulers timer handler for the CPU.
 *
 * @param ctx The `sched_cpu_ctx_t` structure to initialize.
 * @param self The `cpu_t` structure associated with this scheduling context.
 */
void sched_cpu_ctx_init(sched_cpu_ctx_t* ctx, cpu_t* self);

/**
 * @brief The idle loop for a CPU.
 *
 * The `sched_idle_loop()` function is the main loop where idle threads execute. The boot thread will eventually end up
 * here to.
 *
 */
NORETURN extern void sched_idle_loop(void);

/**
 * @brief Specify that the boot thread is no longer needed.
 *
 * The `sched_done_with_boot_thread()` function is called when the kernel has finished booting and the boot thread is no
 * longer needed, instead of just discarding it, the boot thread becomes the idle thread of the bootstrap cpu.
 *
 * Additionally, this function will validate that at least one timer source, IRQ chip, and IPI chip is registered, as
 * those are required for the scheduler to function properly.
 *
 */
NORETURN void sched_done_with_boot_thread(void);

/**
 * @brief Puts the current thread to sleep.
 *
 * @param timeout The maximum time to sleep. If `CLOCKS_NEVER`, it sleeps forever.
 * @return On success, `0`. On error, `ERR` and `errno` is set.
 */
uint64_t sched_nanosleep(clock_t timeout);

/**
 * @brief Checks if the CPU is idle.
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
 * Will not increment the reference count of the returned process, as we consider the currently running thread to always
 * be referencing its process.
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
 * Will not increment the reference count of the returned process, as we consider the currently running thread to always
 * be referencing its process.
 *
 * @return The process of the currently running thread.
 */
process_t* sched_process_unsafe(void);

/**
 * @brief Exits the current process.
 *
 * The `sched_process_exit()` function terminates the currently executing process and all its threads. Note that this
 * function will never return, instead it triggers an interrupt that kills the current thread.
 *
 * @param status The exit status of the process.
 */
_NORETURN void sched_process_exit(uint64_t status);

/**
 * @brief Exits the current thread.
 *
 * The `sched_thread_exit()` function terminates the currently executing thread. Note that this function will never
 * return, instead it triggers an interrupt that kills the thread.
 *
 */
_NORETURN void sched_thread_exit(void);

/**
 * @brief Yields the CPU to another thread.
 *
 * The `sched_yield()` function voluntarily relinquishes the currently running threads time slice, and invokes the
 * scheduler.
 *
 */
void sched_yield(void);

/**
 * @brief Pushes a thread onto a scheduling queue.
 *
 * This will take ownership of the thread, so the caller should not deref it after calling this function as if a
 * preemption occurs it could be freed by the time the function returns.
 *
 * @param thread The thread to be pushed.
 * @param target The target cpu that the thread should run on, can be `NULL` to specify the currently running cpu-
 */
void sched_push(thread_t* thread, cpu_t* target);

/**
 * @brief Pushes a newly created thread onto the scheduling queue.
 *
 * This will take ownership of the thread, so the caller should not deref it after calling this function as if a
 * preemption occurs it could be freed by the time the function returns.
 *
 * @param thread The thread to be pushed.
 * @param parent The parent thread.
 */
void sched_push_new_thread(thread_t* thread, thread_t* parent);

/**
 * @brief The main scheduling function.
 *
 * Must be called in an interrupt context.
 *
 * @param frame The current interrupt frame.
 * @param self The currently running cpu.
 */
void sched_do(interrupt_frame_t* frame, cpu_t* self);

/** @} */
