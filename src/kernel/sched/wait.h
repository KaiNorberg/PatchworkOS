#pragma once

#include "sync/lock.h"

#include <sys/list.h>
#include <sys/proc.h>

#define WAIT_ALL UINT64_MAX

// Blocks untill condition is true, condition will be tested after every call to wait_unblock.
#define WAIT_BLOCK(waitQueue, condition) \
    ({ \
        wait_result_t result = WAIT_NORM; \
        while (!(condition) && result == WAIT_NORM) \
        { \
            result = wait_block(waitQueue, CLOCKS_NEVER); \
        } \
        result; \
    })

// Blocks untill condition is true, condition will be tested after every call to wait_unblock.
// Will also return after timeout is reached, timeout will be reached even if wait_unblock is never called.
#define WAIT_BLOCK_TIMEOUT(waitQueue, condition, timeout) \
    ({ \
        wait_result_t result = WAIT_NORM; \
        clock_t uptime = systime_uptime(); \
        clock_t deadline = (timeout) == CLOCKS_NEVER ? CLOCKS_NEVER : (timeout) + uptime; \
        while (!(condition) && result == WAIT_NORM) \
        { \
            if (deadline <= uptime) \
            { \
                result = WAIT_TIMEOUT; \
                break; \
            } \
            clock_t remaining = deadline == CLOCKS_NEVER ? CLOCKS_NEVER : (deadline > uptime ? deadline - uptime : 0); \
            result = wait_block(waitQueue, remaining); \
            uptime = systime_uptime(); \
        } \
        result; \
    })

// Blocks untill condition is true, condition will be tested after every call to wait_unblock.
// Should be called with lock acquired, will release lock before blocking and return with lock acquired.
#define WAIT_BLOCK_LOCK(waitQueue, lock, condition) \
    ({ \
        wait_result_t result = WAIT_NORM; \
        while (!(condition) && result == WAIT_NORM) \
        { \
            result = wait_block_lock(waitQueue, CLOCKS_NEVER, lock); \
        } \
        result; \
    })

// Blocks untill condition is true, condition will be tested after every call to wait_unblock.
// Should be called with lock acquired, will release lock before blocking and return with lock acquired.
// Will also return after timeout is reached, timeout will be reached even if wait_unblock is never called.
#define WAIT_BLOCK_LOCK_TIMEOUT(waitQueue, lock, condition, timeout) \
    ({ \
        wait_result_t result = WAIT_NORM; \
        clock_t uptime = systime_uptime(); \
        clock_t deadline = (timeout) == CLOCKS_NEVER ? CLOCKS_NEVER : (timeout) + uptime; \
        while (!(condition) && result == WAIT_NORM) \
        { \
            if (deadline <= uptime) \
            { \
                result = WAIT_TIMEOUT; \
                break; \
            } \
            clock_t remaining = deadline == CLOCKS_NEVER ? CLOCKS_NEVER : (deadline > uptime ? deadline - uptime : 0); \
            result = wait_block_lock(waitQueue, remaining, lock); \
            uptime = systime_uptime(); \
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
    WAIT_NORM = 0,
    WAIT_TIMEOUT = 1,
    WAIT_NOTE = 2,
    WAIT_ERROR = 3
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

void wait_queue_init(wait_queue_t* waitQueue);

void wait_queue_deinit(wait_queue_t* waitQueue);

void wait_thread_ctx_init(wait_thread_ctx_t* wait);

void wait_cpu_ctx_init(wait_cpu_ctx_t* wait);

clock_t wait_next_deadline(trap_frame_t* trapFrame, cpu_t* self);

void wait_timer_trap(trap_frame_t* trapFrame, cpu_t* self);

bool wait_finalize_block(trap_frame_t* trapFrame, cpu_t* self, thread_t* thread);

void wait_unblock_thread(thread_t* thread, wait_result_t result);

uint64_t wait_unblock(wait_queue_t* waitQueue, uint64_t amount);

wait_result_t wait_block(wait_queue_t* waitQueue, clock_t timeout);

// Should be called with lock acquired, will release lock after blocking then reacquire it before returning from the
// function.
wait_result_t wait_block_lock(wait_queue_t* waitQueue, clock_t timeout, lock_t* lock);

wait_result_t wait_block_many(wait_queue_t** waitQueues, uint64_t amount, clock_t timeout);