#pragma once

#include "lock.h"
#include "thread.h"

#include <sys/list.h>

// Blocks untill condition is true, condition will be tested after every call to waitsys_unblock.
#define WAITSYS_BLOCK(waitQueue, condition) \
    ({ \
        block_result_t result = BLOCK_NORM; \
        while (!(condition) && result == BLOCK_NORM) \
        { \
            result = waitsys_block(waitQueue, NEVER); \
        } \
        result; \
    })

// Blocks untill condition is true, condition will be tested after every call to waitsys_unblock.
// Will also return after timeout is reached, timeout will be reached even if waitsys_unblock is never called.
#define WAITSYS_BLOCK_TIMEOUT(waitQueue, condition, timeout) \
    ({ \
        block_result_t result = BLOCK_NORM; \
        nsec_t uptime = systime_uptime(); \
        nsec_t deadline = (timeout) == NEVER ? NEVER : (timeout) + uptime; \
        while (!(condition) && result == BLOCK_NORM) \
        { \
            if (deadline < uptime) \
            { \
                result = BLOCK_TIMEOUT; \
                break; \
            } \
            nsec_t remaining = deadline == NEVER ? NEVER : (deadline > uptime ? deadline - uptime : 0); \
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
        while (result == BLOCK_NORM) \
        { \
            if (condition) \
            { \
                break; \
            } \
            lock_release(lock); \
            result = waitsys_block(waitQueue, NEVER); \
            lock_acquire(lock); \
        } \
        result; \
    })

// Blocks untill condition is true, condition will be tested after every call to waitsys_unblock.
// When condition is tested it will also acquire lock, and the macro will always return with lock still acquired.
// Will also return after timeout is reached, timeout will be reached even if waitsys_unblock is never called.
#define WAITSYS_BLOCK_LOCK_TIMEOUT(waitQueue, lock, condition, timeout) \
    ({ \
        block_result_t result = BLOCK_NORM; \
        nsec_t uptime = systime_uptime(); \
        nsec_t deadline = (timeout) == NEVER ? NEVER : (timeout) + uptime; \
        lock_acquire(lock); \
        while (result == BLOCK_NORM) \
        { \
            if (deadline < uptime) \
            { \
                result = BLOCK_TIMEOUT; \
                break; \
            } \
            else if (condition) \
            { \
                break; \
            } \
            lock_release(lock); \
            nsec_t remaining = deadline == NEVER ? NEVER : (deadline > uptime ? deadline - uptime : 0); \
            result = waitsys_block(waitQueue, remaining); \
            uptime = systime_uptime(); \
            lock_acquire(lock); \
        } \
        result; \
    })

typedef struct wait_queue_entry
{
    list_entry_t entry;
    thread_t* thread;
    wait_queue_t* waitQueue;
} wait_queue_entry_t;

typedef struct wait_queue
{
    lock_t lock;
    list_t entries;
} wait_queue_t;

void wait_queue_init(wait_queue_t* waitQueue);

void wait_queue_deinit(wait_queue_t* waitQueue);

void waitsys_init(void);

void waitsys_update_trap(trap_frame_t* trapFrame);

void waitsys_block_trap(trap_frame_t* trapFrame);

block_result_t waitsys_block(wait_queue_t* waitQueue, nsec_t timeout);

block_result_t waitsys_block_many(wait_queue_t** waitQueues, uint64_t amount, nsec_t timeout);

void waitsys_unblock(wait_queue_t* waitQueue);