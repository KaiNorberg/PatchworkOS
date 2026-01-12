#pragma once

#include <kernel/cpu/regs.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/sync/lock.h>

#include <errno.h>
#include <sys/list.h>
#include <sys/proc.h>

typedef struct thread thread_t;
typedef struct cpu cpu_t;

typedef struct wait_entry wait_entry_t;
typedef struct wait_queue wait_queue_t;
typedef struct wait_client wait_client_t;
typedef struct wait wait_t;

/**
 * @brief Wait queue implementation.
 * @defgroup kernel_sched_wait Waiting subsystem
 * @ingroup kernel_sched
 *
 * The waiting subsystem provides threads with the ability to suspend their execution until a certain condition is met
 * and/or a timeout occurs.
 *
 * The common usage pattern is to call `WAIT_BLOCK()` to check for a specified condition, when that condition is
 * modified the subsystem utilizing the wait queue is expected to call `wait_unblock()` to wake up a specified number of
 * waiting threads, causing them to re-evaluate the condition. If the condition is still not met the thread will go back
 * to sleep, otherwise it will continue executing.
 *
 * @note Generally its preferred to use the `WAIT_BLOCK*` macros instead of directly calling the functions provided by
 * this subsystem.
 *
 * @{
 */

/**
 * @brief Used to indicate that the wait should unblock all waiting threads.
 */
#define WAIT_ALL UINT64_MAX

/**
 * @brief Blocks until the condition is true, will test the condition on every wakeup.
 *
 * @return On success, `0`. On error, `ERR` and `errno` is set to:
 * - Check `wait_block_commit()`.
 */
#define WAIT_BLOCK(queue, condition) \
    ({ \
        assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE); \
        uint64_t result = 0; \
        while (!(condition) && result == 0) \
        { \
            wait_queue_t* temp = queue; \
            if (wait_block_prepare(&temp, 1, CLOCKS_NEVER) == ERR) \
            { \
                result = ERR; \
                break; \
            } \
            result = wait_block_commit(); \
        } \
        result; \
    })

/**
 * @brief Blocks until the condition is true, condition will be tested on every wakeup. Reaching the timeout will always
 * unblock.
 *
 * @return On success, `0`. On error, `ERR` and `errno` is set to:
 * - Check `wait_block_commit()`.
 */
#define WAIT_BLOCK_TIMEOUT(queue, condition, timeout) \
    ({ \
        assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE); \
        uint64_t result = 0; \
        clock_t uptime = clock_uptime(); \
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
            wait_queue_t* temp = queue; \
            if (wait_block_prepare(&temp, 1, remaining) == ERR) \
            { \
                result = ERR; \
                break; \
            } \
            result = wait_block_commit(); \
            uptime = clock_uptime(); \
        } \
        result; \
    })

/**
 * @brief Blocks until the condition is true, condition will be tested on every wakeup. Will release the lock before
 * blocking and acquire it again after waking up.
 *
 * @return On success, `0`. On error, `ERR` and `errno` is set to:
 * - Check `wait_block_commit()`.
 */
#define WAIT_BLOCK_LOCK(queue, lock, condition) \
    ({ \
        assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE)); \
        uint64_t result = 0; \
        while (!(condition) && result == 0) \
        { \
            wait_queue_t* temp = queue; \
            if (wait_block_prepare(&temp, 1, CLOCKS_NEVER) == ERR) \
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
 * @brief Blocks until the condition is true, condition will be tested on every wakeup. Will release the lock before
 * blocking and acquire it again after waking up. Reaching the timeout will always unblock.
 *
 * @return On success, `0`. On error, `ERR` and `errno` is set to:
 * - Check `wait_block_commit()`.
 */
#define WAIT_BLOCK_LOCK_TIMEOUT(queue, lock, condition, timeout) \
    ({ \
        uint64_t result = 0; \
        clock_t uptime = clock_uptime(); \
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
            wait_queue_t* temp = queue; \
            if (wait_block_prepare(&temp, 1, remaining) == ERR) \
            { \
                result = ERR; \
                break; \
            } \
            lock_release(lock); \
            result = wait_block_commit(); \
            assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE); \
            lock_acquire(lock); \
            uptime = clock_uptime(); \
        } \
        result; \
    })

/**
 * @brief Represents a thread waiting on a wait queue.
 * @struct wait_entry_t
 *
 * Since each thread can wait on multiple wait queues simultaneously, each wait queue the thread is waiting on
 * will have its own wait entry.
 */
typedef struct wait_entry
{
    list_entry_t queueEntry;  ///< Used in wait_queue_t->entries.
    list_entry_t threadEntry; ///< Used in wait_client_t->entries.
    thread_t* thread;         ///< The thread that is waiting.
    wait_queue_t* queue;      ///< The wait queue the thread is waiting on.
} wait_entry_t;

/**
 * @brief The primitive that threads block on.
 * @struct wait_queue_t
 */
typedef struct wait_queue
{
    lock_t lock;
    list_t entries; ///< List of wait entries for threads waiting on this queue.
} wait_queue_t;

/**
 * @brief Represents a thread in the waiting subsystem.
 * @struct wait_client_t
 *
 * Each thread stores all wait queues it is currently waiting on in here to allow blocking on multiple wait queues,
 * since if one queue unblocks the thread must be removed from all other queues as well.
 */
typedef struct wait_client
{
    list_entry_t entry;
    list_t entries;   ///< List of wait entries, one for each wait queue the thread is waiting on.
    errno_t err;      ///< Error number set when unblocking the thread, `EOK` for no error.
    clock_t deadline; ///< Deadline for timeout, `CLOCKS_NEVER` for no timeout.
    wait_t* owner;    ///< The wait cpu context of the cpu the thread is blocked on.
} wait_client_t;

/**
 * @brief Represents one instance of the waiting subsystem for a CPU.
 * @struct wait_t
 */
typedef struct wait
{
    list_t blockedThreads; ///< List of blocked threads, sorted by deadline.
    lock_t lock;
} wait_t;

/**
 * @brief Create a wait queue initializer.
 *
 * @param name The name of the wait queue variable.
 * @return The wait queue initializer.
 */
#define WAIT_QUEUE_CREATE(name) {.lock = LOCK_CREATE(), .entries = LIST_CREATE(name.entries)}

/**
 * @brief Initialize wait queue.
 *
 * @param queue The wait queue to initialize.
 */
void wait_queue_init(wait_queue_t* queue);

/**
 * @brief Deinitialize wait queue.
 *
 * @param queue The wait queue to deinitialize.
 */
void wait_queue_deinit(wait_queue_t* queue);

/**
 * @brief Initialize a threads wait client.
 *
 * @param client The wait client to initialize.
 */
void wait_client_init(wait_client_t* client);

/**
 * @brief Initialize an instance of the waiting subsystem.
 *
 * @param wait The instance to initialize.
 */
void wait_init(wait_t* wait);

/**
 * @brief Check for timeouts and unblock threads as needed.
 *
 * Will be called by the `interrupt_handler()`.
 *
 * @param frame The interrupt frame.
 * @param self The current CPU.
 */
void wait_check_timeouts(interrupt_frame_t* frame, cpu_t* self);

/**
 * @brief Prepare to block the currently running thread.
 *
 * Needed to handle race conditions when a thread is unblocked prematurely. The following sequence is used:
 * - Call `wait_block_prepare()` to add the currently running thread to the provided wait queues and disable interrupts.
 * - Check if the condition to block is still valid.
 * - (The condition might change here, thus causing a race condition, leading to premature unblocking.)
 * - If the condition was evaluated as not valid, call `wait_block_cancel()`.
 * - If the condition was evaluated as valid, call `wait_block_commit()` to block the thread. If the thread was
 * unblocked prematurely this function will return immediately.
 *
 * Will reenable interrupts on failure.
 *
 * @param waitQueues Array of wait queues to add the thread to.
 * @param amount Number of wait queues to add the thread to.
 * @param timeout Timeout.
 * @return On success, `0`. On failure, returns `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid arguments.
 * - `ENOMEM`: Out of memory.
 */
uint64_t wait_block_prepare(wait_queue_t** waitQueues, uint64_t amount, clock_t timeout);

/**
 * @brief Cancels blocking of the currently running thread.
 *
 * Should be called after `wait_block_prepare()` has been called if the condition to block is no longer valid.
 *
 * Will reenable interrupts.
 */
void wait_block_cancel(void);

/**
 * @brief Block the currently running thread.
 *
 * Should be called after `wait_block_prepare()`. If the thread was unblocked prematurely this function will return
 * immediately.
 *
 * Will reenable interrupts.
 *
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `ETIMEDOUT`: The thread timed out.
 * - `EINTR`: The thread was interrupted by a note.
 * - Other error codes as set by the subsystem utilizing the wait queue.
 */
uint64_t wait_block_commit(void);

/**
 * @brief Finalize blocking of a thread.
 *
 * When `wait_block_commit()` is called the scheduler will be invoked, the scheduler will then call this function to
 * finalize the blocking of the thread.
 *
 * Its possible that during the gap between `wait_block_commit()` and this function being called the thread was
 * prematurely unblocked, in that case this function will return false and the scheduler will resume the thread
 * immediately.
 *
 * @param frame The interrupt frame.
 * @param self The CPU the thread is being blocked on.
 * @param thread The thread to block.
 * @param uptime The current uptime.
 * @return `true` if the thread was blocked, `false` if the thread was prematurely unblocked.
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
 * @param queue The wait queue to unblock threads from.
 * @param amount The number of threads to unblock or `WAIT_ALL` to unblock all threads.
 * @param err The errno value to set for the unblocked threads or `EOK` for no error.
 * @return The number of threads that were unblocked.
 */
uint64_t wait_unblock(wait_queue_t* queue, uint64_t amount, errno_t err);

/** @} */
