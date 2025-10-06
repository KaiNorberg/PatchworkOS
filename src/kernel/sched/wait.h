#pragma once

#include "sync/lock.h"

#include <common/regs.h>

#include <sys/list.h>
#include <sys/proc.h>

/**
 * @brief Wait queue implementation.
 * @defgroup kernel_sched_wait Wait
 * @ingroup kernel_sched
 *
 */

#define WAIT_ALL UINT64_MAX

/**
 * @brief Basic block.
 * @ingroup kernel_sched_wait
 *
 * Blocks untill condition is true, condition will be tested after every time the thread wakes up.
 *
 * @return wait_result_t Check 'wait_result_t' definition for more.
 */
#define WAIT_BLOCK(waitQueue, condition) \
    ({ \
        assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE); \
        wait_result_t result = WAIT_NORM; \
        while (!(condition) && result == WAIT_NORM) \
        { \
            wait_queue_t* temp = waitQueue; \
            if (wait_block_setup(&temp, 1, CLOCKS_NEVER) == ERR) \
            { \
                result = WAIT_ERROR; \
                break; \
            } \
            result = wait_block_do(); \
        } \
        result; \
    })

/**
 * @brief Block with timeout.
 * @ingroup kernel_sched_wait
 *
 * Blocks untill condition is true, condition will be tested after every time the thread wakes up.
 * Will also return after timeout is reached, the thread will automatically wake up whence the timeout is reached.
 *
 * @return wait_result_t Check 'wait_result_t' definition for more.
 */
#define WAIT_BLOCK_TIMEOUT(waitQueue, condition, timeout) \
    ({ \
        assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE); \
        wait_result_t result = WAIT_NORM; \
        clock_t uptime = timer_uptime(); \
        clock_t deadline = (timeout) == CLOCKS_NEVER ? CLOCKS_NEVER : (timeout) + uptime; \
        while (!(condition) && result == WAIT_NORM) \
        { \
            if (deadline <= uptime) \
            { \
                result = WAIT_TIMEOUT; \
                break; \
            } \
            clock_t remaining = deadline == CLOCKS_NEVER ? CLOCKS_NEVER : (deadline > uptime ? deadline - uptime : 0); \
            wait_queue_t* temp = waitQueue; \
            if (wait_block_setup(&temp, 1, remaining) == ERR) \
            { \
                result = WAIT_ERROR; \
                break; \
            } \
            result = wait_block_do(); \
            uptime = timer_uptime(); \
        } \
        result; \
    })

/**
 * @brief Block with a spinlock.
 * @ingroup kernel_sched_wait
 *
 * Blocks untill condition is true, condition will be tested after every time the thread wakes up.
 * Should be called with the lock acquired, will release the lock before blocking and return with the lock acquired.
 *
 * @return wait_result_t Check 'wait_result_t' definition for more.
 */
#define WAIT_BLOCK_LOCK(waitQueue, lock, condition) \
    ({ \
        assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE)); \
        wait_result_t result = WAIT_NORM; \
        while (!(condition) && result == WAIT_NORM) \
        { \
            wait_queue_t* temp = waitQueue; \
            if (wait_block_setup(&temp, 1, CLOCKS_NEVER) == ERR) \
            { \
                result = WAIT_ERROR; \
                break; \
            } \
            lock_release(lock); \
            result = wait_block_do(); \
            assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE); \
            lock_acquire(lock); \
        } \
        result; \
    })

/**
 * @brief Block with a spinlock and timeout.
 * @ingroup kernel_sched_wait
 *
 * Blocks untill condition is true, condition will be tested after every call to wait_unblock.
 * Should be called with lock acquired, will release lock before blocking and return with lock acquired.
 * Will also return after timeout is reached, timeout will be reached even if wait_unblock is never called.
 *
 * @return wait_result_t Check 'wait_result_t' definition for more.
 */
#define WAIT_BLOCK_LOCK_TIMEOUT(waitQueue, lock, condition, timeout) \
    ({ \
        wait_result_t result = WAIT_NORM; \
        clock_t uptime = timer_uptime(); \
        clock_t deadline = (timeout) == CLOCKS_NEVER ? CLOCKS_NEVER : (timeout) + uptime; \
        while (!(condition) && result == WAIT_NORM) \
        { \
            if (deadline <= uptime) \
            { \
                result = WAIT_TIMEOUT; \
                break; \
            } \
            clock_t remaining = deadline == CLOCKS_NEVER ? CLOCKS_NEVER : (deadline > uptime ? deadline - uptime : 0); \
            wait_queue_t* temp = waitQueue; \
            if (wait_block_setup(&temp, 1, remaining) == ERR) \
            { \
                result = WAIT_ERROR; \
                break; \
            } \
            lock_release(lock); \
            result = wait_block_do(); \
            assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE); \
            lock_acquire(lock); \
            uptime = timer_uptime(); \
        } \
        result; \
    })

typedef struct thread thread_t;
typedef struct cpu cpu_t;

typedef struct wait_queue
{
    lock_t lock;
    list_t entries;
} wait_queue_t;

typedef struct wait_entry
{
    list_entry_t queueEntry;  // Used in wait_queue_t->entries
    list_entry_t threadEntry; // Used in wait_thread_ctx_t->entries
    thread_t* thread;
    wait_queue_t* waitQueue;
} wait_entry_t;

typedef enum
{
    WAIT_NORM = 0,    ///< Normal wait result
    WAIT_TIMEOUT = 1, ///< Wait timed out
    WAIT_NOTE = 2,    ///< Wait was interrupted by a note
    WAIT_ERROR = 3    ///< Wait encountered an error
} wait_result_t;

typedef struct
{
    list_t entries;
    uint8_t entryAmount;
    wait_result_t result;
    clock_t deadline;
    cpu_t* owner;
} wait_thread_ctx_t;

typedef struct
{
    list_t blockedThreads;
    lock_t lock;
} wait_cpu_ctx_t;

#define WAIT_QUEUE_CREATE {.lock = LOCK_CREATE, .entries = LIST_CREATE}

void wait_init(void);

void wait_queue_init(wait_queue_t* waitQueue);
void wait_queue_deinit(wait_queue_t* waitQueue);

void wait_thread_ctx_init(wait_thread_ctx_t* wait);

void wait_cpu_ctx_init(wait_cpu_ctx_t* wait);

clock_t wait_next_deadline(trap_frame_t* trapFrame, cpu_t* self);

bool wait_block_finalize(trap_frame_t* trapFrame, cpu_t* self, thread_t* thread);

void wait_unblock_thread(thread_t* thread, wait_result_t result);

uint64_t wait_unblock(wait_queue_t* waitQueue, uint64_t amount);

/**
 * @brief Setup blocking but dont block yet.
 * @ingroup kernel_sched_wait
 *
 * The `wait_block_setup()` function adds the currently running thread to the provided wait queues, sets the threads
 * state and disables interrupts. But it does not yet actually block, the thread will continue executing code and will
 * return from the function.
 *
 * @param waitQueues Array of wait queues to add the thread to.
 * @param amount Number of wait queues to add the thread to.
 * @param timeout Timeout.
 * @return On success, 0. On failure, interrupts are reenabled, returns ERR and errno is set.
 */
uint64_t wait_block_setup(wait_queue_t** waitQueues, uint64_t amount, clock_t timeout);

/**
 * @brief Cancel blocking.
 * @ingroup kernel_sched_wait
 *
 * The `wait_block_cancel()` function cancels the blocking of the currently running thread. Should only be called after
 * `wait_block_setup()` has been called. It removes the thread from the wait queues and sets the threads state to
 * `THREAD_RUNNING`. It also always enables interrupts.
 *
 */
void wait_block_cancel(wait_result_t result);

/**
 * @brief Block the currently running thread.
 * @ingroup kernel_sched_wait
 *
 * The `wait_block_do()` function blocks the currently running thread. Should only be called after `wait_block_setup()`
 * has been called. It removes the thread from the wait queues and sets the threads state to `THREAD_BLOCKED`. When the
 * thread is rescheduled interrupts will be enabled.
 *
 * @return wait_result_t Check 'wait_result_t' definition for more.
 */
wait_result_t wait_block_do(void);
