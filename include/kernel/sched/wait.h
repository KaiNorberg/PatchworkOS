#pragma once

#include <kernel/cpu/regs.h>
#include <kernel/sync/lock.h>

#include <errno.h>
#include <sys/list.h>
#include <sys/proc.h>

typedef struct thread thread_t;
typedef struct cpu cpu_t;

/**
 * @brief Wait queue implementation.
 * @defgroup kernel_sched_wait Waiting subsystem
 * @ingroup kernel_sched
 *
 * @{
 */

/**
 * @brief Wait for all.
 *
 * Used to indicate that the wait should unblock when all wait queues have unblocked the thread.
 */
#define WAIT_ALL UINT64_MAX

/**
 * @brief Basic block.
 *
 * Blocks until condition is true, condition will be tested after every time the thread wakes up.
 *
 * Check `wait_block_commit()` for errno values.
 *
 * @return On success, `0`. On error, `ERR` and `errno` is set.
 */
#define WAIT_BLOCK(waitQueue, condition) \
    ({ \
        assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE); \
        uint64_t result = 0; \
        while (!(condition) && result == 0) \
        { \
            wait_queue_t* temp = waitQueue; \
            if (wait_block_setup(&temp, 1, CLOCKS_NEVER) == ERR) \
            { \
                result = ERR; \
                break; \
            } \
            result = wait_block_commit(); \
        } \
        result; \
    })

/**
 * @brief Block with timeout.
 *
 * Blocks untill condition is true, condition will be tested after every time the thread wakes up.
 * Will also return after timeout is reached, the thread will automatically wake up whence the timeout is reached.
 *
 * Check `wait_block_commit()` for errno values.
 *
 * @return On success, `0`. On error, `ERR` and `errno` is set.
 */
#define WAIT_BLOCK_TIMEOUT(waitQueue, condition, timeout) \
    ({ \
        assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE); \
        uint64_t result = 0; \
        clock_t uptime = sys_time_uptime(); \
        clock_t deadline = CLOCKS_DEADLINE(timeout, uptime); \
        while (!(condition) && result == 0) \
        { \
            if (deadline <= uptime) \
            { \
                errno = ETIMEDOUT; \
                result = ERR; \
                break; \
            } \
            clock_t remaining = CLOCKS_REMAINING(deadline, uptime); \
            wait_queue_t* temp = waitQueue; \
            if (wait_block_setup(&temp, 1, remaining) == ERR) \
            { \
                result = ERR; \
                break; \
            } \
            result = wait_block_commit(); \
            uptime = sys_time_uptime(); \
        } \
        result; \
    })

/**
 * @brief Block with a spinlock.
 *
 * Blocks untill condition is true, condition will be tested after every time the thread wakes up.
 * Should be called with the lock acquired, will release the lock before blocking and return with the lock acquired.
 *
 * Check `wait_block_commit()` for errno values.
 *
 * @return On success, `0`. On error, `ERR` and `errno` is set.
 */
#define WAIT_BLOCK_LOCK(waitQueue, lock, condition) \
    ({ \
        assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE)); \
        uint64_t result = 0; \
        while (!(condition) && result == 0) \
        { \
            wait_queue_t* temp = waitQueue; \
            if (wait_block_setup(&temp, 1, CLOCKS_NEVER) == ERR) \
            { \
                result = ERR; \
                break; \
            } \
            lock_release(lock); \
            result = wait_block_commit(); \
            assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE); \
            lock_acquire(lock); \
        } \
        result; \
    })

/**
 * @brief Block with a spinlock and timeout.
 *
 * Blocks untill condition is true, condition will be tested after every call to wait_unblock.
 * Should be called with lock acquired, will release lock before blocking and return with lock acquired.
 * Will also return after timeout is reached, timeout will be reached even if wait_unblock is never called.
 *
 * Check `wait_block_commit()` for errno values.
 *
 * @return On success, `0`. On error, `ERR` and `errno` is set.
 */
#define WAIT_BLOCK_LOCK_TIMEOUT(waitQueue, lock, condition, timeout) \
    ({ \
        uint64_t result = 0; \
        clock_t uptime = sys_time_uptime(); \
        clock_t deadline = CLOCKS_DEADLINE(timeout, uptime); \
        while (!(condition) && result == ERR) \
        { \
            if (deadline <= uptime) \
            { \
                errno = ETIMEDOUT; \
                result = ERR; \
                break; \
            } \
            clock_t remaining = CLOCKS_REMAINING(deadline, uptime); \
            wait_queue_t* temp = waitQueue; \
            if (wait_block_setup(&temp, 1, remaining) == ERR) \
            { \
                result = ERR; \
                break; \
            } \
            lock_release(lock); \
            result = wait_block_commit(); \
            assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE); \
            lock_acquire(lock); \
            uptime = sys_time_uptime(); \
        } \
        result; \
    })

/**
 * @brief Wait queue structure.
 * @struct wait_queue_t
 */
typedef struct wait_queue
{
    lock_t lock;
    list_t entries; ///< List of wait entries for threads waiting on this queue.
} wait_queue_t;

/**
 * @brief Per-thread wait entry.
 * @struct wait_entry_t
 *
 * Each thread waiting on a wait queue will have one of these entries in the wait queue's list of entries as well as in
 * the thread's list of wait entries.
 */
typedef struct wait_entry
{
    list_entry_t queueEntry;  ///< Used in wait_queue_t->entries.
    list_entry_t threadEntry; ///< Used in wait_thread_ctx_t->entries.
    thread_t* thread;         ///< The thread that is waiting.
    wait_queue_t* waitQueue;  ///< The wait queue the thread is waiting on.
} wait_entry_t;

/**
 * @brief Per-CPU wait context.
 * @struct wait_cpu_ctx_t
 *
 * Each cpu stores all threads that were blocked on it, sorted by deadline, to handle timeouts.
 */
typedef struct
{
    list_t blockedThreads; ///< List of blocked threads, sorted by deadline.
    cpu_t* cpu;            ///< The CPU this context belongs to.
    lock_t lock;
} wait_cpu_ctx_t;

/**
 * @brief Per-thread wait context.
 * @struct wait_thread_ctx_t
 *
 * Each thread stores all wait queues it is currently waiting on in here to allow blocking on multiple wait queues,
 * since if one queue unblocks the thread must be removed from all other queues as well.
 */
typedef struct
{
    list_t entries;      ///< List of wait entries, one for each wait queue the thread is waiting on.
    errno_t err;         ///< Error number set when unblocking the thread, `EOK` for no error.
    clock_t deadline;    ///< Deadline for timeout, `CLOCKS_NEVER` for no timeout.
    wait_cpu_ctx_t* cpu; ///< The wait cpu context of the cpu the thread is blocked on.
} wait_thread_ctx_t;

/**
 * @brief Create a wait queue initializer.
 *
 * @param name The name of the wait queue variable.
 * @return The wait queue initializer.
 */
#define WAIT_QUEUE_CREATE(name) {.lock = LOCK_CREATE, .entries = LIST_CREATE(name.entries)}

/**
 * @brief Initialize wait queue.
 *
 * @param waitQueue The wait queue to initialize.
 */
void wait_queue_init(wait_queue_t* waitQueue);

/**
 * @brief Deinitialize wait queue.
 *
 * @param waitQueue The wait queue to deinitialize.
 */
void wait_queue_deinit(wait_queue_t* waitQueue);

/**
 * @brief Initialize per-thread wait context.
 *
 * @param wait The thread wait context to initialize.
 */
void wait_thread_ctx_init(wait_thread_ctx_t* wait);

/**
 * @brief Initialize per-CPU wait context.
 *
 * Must be called on the CPU the context belongs to.
 *
 * @param wait The CPU wait context to initialize.
 * @param self The CPU the context belongs to.
 */
void wait_cpu_ctx_init(wait_cpu_ctx_t* wait, cpu_t* self);

/**
 * @brief Check for timeouts and unblock threads as needed.
 *
 * @param frame The interrupt frame.
 * @param self The current CPU.
 */
void wait_check_timeouts(interrupt_frame_t* frame, cpu_t* self);

/**
 * @brief Finalize blocking of a thread.
 *
 * When `wait_block_commit()` is called the thread will schedule, the scheduler will then call this function to finalize
 * the blocking of the thread.
 *
 * Its possible that during the gap between `wait_block_commit()` and this function being called the thread was
 * unblocked already, in that case this function will return false and the thread will not be blocked.
 *
 * This function will add the thread to the cpu's `blockedThreads` list to handle timeouts.
 *
 * @param frame The interrupt frame.
 * @param self The CPU the thread is being blocked on.
 * @param thread The thread to block.
 * @param uptime The current uptime.
 * @return `true` if the thread was blocked, `false` if the thread was already unblocked.
 */
bool wait_block_finalize(interrupt_frame_t* frame, cpu_t* self, thread_t* thread, clock_t uptime);

/**
 * @brief Unblock a specific thread.
 *
 * Unblocks the provided thread, removing it from all wait queues it is waiting on.
 *
 * The thread must be in the `THREAD_UNBLOCKING` state when this function is called.
 *
 * @param thread The thread to unblock.
 * @param err The errno value to set for the thread or `EOK` for no error.
 */
void wait_unblock_thread(thread_t* thread, errno_t err);

/**
 * @brief Unblock threads waiting on a wait queue.
 *
 * @param waitQueue The wait queue to unblock threads from.
 * @param amount The number of threads to unblock or `WAIT_ALL` to unblock all threads.
 * @param err The errno value to set for the unblocked threads or `EOK` for no error.
 * @return The number of threads that were unblocked.
 */
uint64_t wait_unblock(wait_queue_t* waitQueue, uint64_t amount, errno_t err);

/**
 * @brief Setup blocking but dont block yet.
 *
 * Adds the currently running thread to the provided wait queues, sets the threads state and disables interrupts. But it
 * does not yet actually block, and it does not add the thread to its cpus `blockedThreads` list, the thread will
 * continue executing code and will return from the function.
 *
 * @param waitQueues Array of wait queues to add the thread to.
 * @param amount Number of wait queues to add the thread to.
 * @param timeout Timeout.
 * @return On success, `0`. On failure, interrupts are reenabled, returns `ERR` and `errno` is set.
 */
uint64_t wait_block_setup(wait_queue_t** waitQueues, uint64_t amount, clock_t timeout);

/**
 * @brief Cancel blocking.
 *
 * Cancels the blocking of the currently running thread. Should only be called after`wait_block_setup()` has been
 * called. It removes the thread from the wait queues and sets the threads state to`THREAD_RUNNING`. It also always
 * enables interrupts.
 */
void wait_block_cancel(void);

/**
 * @brief Block the currently running thread.
 *
 * Blocks the currently running thread. Should only be called after `wait_block_setup()` has been called. It invokes the
 * scheduler which will end up calling `wait_block_finalize()` to finalize the blocking of the thread. Will enable
 * interrupts again when the thread is unblocked.
 *
 * Noteworthy errno values:
 * - `ETIMEDOUT`: The wait timed out.
 * - `EINTR`: The wait was interrupted (probably by a note.)
 *
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t wait_block_commit(void);

/** @} */
