#pragma once

#include "lock.h"
#include "systime.h"

#include <sys/list.h>
#include <sys/proc.h>

// TODO: Make this code less incomprehensible.

#define WAITSYS_ALL UINT64_MAX

// Blocks untill condition is true, condition will be tested after every call to waitsys_unblock.
#define WAITSYS_BLOCK(waitQueue, condition) \
    ({ \
        block_result_t result = BLOCK_NORM; \
        while (!(condition) && result == BLOCK_NORM) \
        { \
            result = waitsys_block(waitQueue, CLOCKS_NEVER); \
        } \
        result; \
    })

// Blocks untill condition is true, condition will be tested after every call to waitsys_unblock.
// Will also return after timeout is reached, timeout will be reached even if waitsys_unblock is never called.
#define WAITSYS_BLOCK_TIMEOUT(waitQueue, condition, timeout) \
    ({ \
        block_result_t result = BLOCK_NORM; \
        clock_t uptime = systime_uptime(); \
        clock_t deadline = (timeout) == CLOCKS_NEVER ? CLOCKS_NEVER : (timeout) + uptime; \
        while (!(condition) && result == BLOCK_NORM) \
        { \
            if (deadline <= uptime) \
            { \
                result = BLOCK_TIMEOUT; \
                break; \
            } \
            clock_t remaining = deadline == CLOCKS_NEVER ? CLOCKS_NEVER : (deadline > uptime ? deadline - uptime : 0); \
            result = waitsys_block(waitQueue, remaining); \
            uptime = systime_uptime(); \
        } \
        result; \
    })

// Blocks untill condition is true, condition will be tested after every call to waitsys_unblock.
// When condition is tested it will also acquire lock, and the macro will always return with lock still acquired.
#define WAITSYS_BLOCK_LOCK(waitQueue, lock, condition) \
    ({ \
        block_result_t result = BLOCK_NORM; \
        lock_acquire(lock); \
        while (!(condition) && result == BLOCK_NORM) \
        { \
            result = waitsys_block_lock(waitQueue, CLOCKS_NEVER, lock); \
        } \
        result; \
    })

// Blocks untill condition is true, condition will be tested after every call to waitsys_unblock.
// When condition is tested it will also acquire lock, and the macro will always return with lock still acquired.
// Will also return after timeout is reached, timeout will be reached even if waitsys_unblock is never called.
#define WAITSYS_BLOCK_LOCK_TIMEOUT(waitQueue, lock, condition, timeout) \
    ({ \
        block_result_t result = BLOCK_NORM; \
        clock_t uptime = systime_uptime(); \
        clock_t deadline = (timeout) == CLOCKS_NEVER ? CLOCKS_NEVER : (timeout) + uptime; \
        lock_acquire(lock); \
        while (!(condition) && result == BLOCK_NORM) \
        { \
            if (deadline <= uptime) \
            { \
                result = BLOCK_TIMEOUT; \
                break; \
            } \
            clock_t remaining = deadline == CLOCKS_NEVER ? CLOCKS_NEVER : (deadline > uptime ? deadline - uptime : 0); \
            result = waitsys_block_lock(waitQueue, remaining, lock); \
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
    list_entry_t threadEntry; // Used in waitsys_thread_ctx_t->entries
    thread_t* thread;
    wait_queue_t* waitQueue;
    bool blocking;
    bool cancelBlock;
} wait_entry_t;

typedef enum
{
    BLOCK_NORM = 0,
    BLOCK_TIMEOUT = 1,
    BLOCK_DEAD = 2,
    BLOCK_ERROR = 3
} block_result_t;

typedef struct
{
    list_t entries;
    uint8_t entryAmount;
    block_result_t result;
    clock_t deadline;
    cpu_t* owner;
} waitsys_thread_ctx_t;

typedef struct
{
    list_t blockedThreads;
    list_t parkedThreads;
    lock_t lock;
} waitsys_cpu_ctx_t;

void wait_queue_init(wait_queue_t* waitQueue);

void wait_queue_deinit(wait_queue_t* waitQueue);

void waitsys_thread_ctx_init(waitsys_thread_ctx_t* waitsys);

void waitsys_cpu_ctx_init(waitsys_cpu_ctx_t* waitsys);

void waitsys_timer_trap(trap_frame_t* trapFrame);

void waitsys_block_trap(trap_frame_t* trapFrame);

void waitsys_unblock(wait_queue_t* waitQueue, uint64_t amount);

block_result_t waitsys_block(wait_queue_t* waitQueue, clock_t timeout);

// Should be called with lock acquired, will release lock after blocking then reacquire it before returning from the
// function.
block_result_t waitsys_block_lock(wait_queue_t* waitQueue, clock_t timeout, lock_t* lock);

block_result_t waitsys_block_many(wait_queue_t** waitQueues, uint64_t amount, clock_t timeout);
