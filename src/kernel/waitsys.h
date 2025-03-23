#pragma once

#include "thread.h"
#include "lock.h"

#include <sys/list.h>

// Blocks untill condition is true, condition will be tested after every call to blocker_unblock.
#define WAITSYS_BLOCK(blocker, condition) \
    ({ \
        block_result_t result = BLOCK_NORM; \
        while (!(condition) && result == BLOCK_NORM) \
        { \
            result = blocker_block(blocker, NEVER); \
        } \
        result; \
    })

// Blocks untill condition is true, condition will be tested after every call to blocker_unblock.
// Will also return after timeout is reached, timeout will be reached even if blocker_unblock is never called.
#define WAITSYS_BLOCK_TIMEOUT(blocker, condition, timeout) \
    ({ \
        block_result_t result = BLOCK_NORM; \
        nsec_t uptime = time_uptime(); \
        nsec_t deadline = (timeout) == NEVER? NEVER : (timeout) + uptime; \
        while (!(condition) && result == BLOCK_NORM) \
        { \
            if (deadline < uptime) \
            { \
                result = BLOCK_TIMEOUT; \
                break; \
            } \
            nsec_t remaining = deadline == NEVER? NEVER : (deadline > uptime? deadline - uptime : 0); \
            result = blocker_block(blocker, remaining); \
            uptime = time_uptime(); \
        } \
        result; \
    })

// Blocks untill condition is true, condition will be tested after every call to blocker_unblock.
// When condition is tested it will also acquire lock, and the macro will always return with lock still acquired.
#define WAITSYS_BLOCK_LOCK(blocker, lock, condition) \
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
            result = blocker_block(blocker, NEVER); \
            lock_acquire(lock); \
        } \
        result; \
    })

// Blocks untill condition is true, condition will be tested after every call to blocker_unblock.
// When condition is tested it will also acquire lock, and the macro will always return with lock still acquired.
// Will also return after timeout is reached, timeout will be reached even if blocker_unblock is never called.
#define WAITSYS_BLOCK_LOCK_TIMEOUT(blocker, lock, condition, timeout) \
    ({ \
        block_result_t result = BLOCK_NORM; \
        nsec_t uptime = time_uptime(); \
        nsec_t deadline = (timeout) == NEVER? NEVER : (timeout) + uptime; \
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
            nsec_t remaining = deadline == NEVER? NEVER : (deadline > uptime? deadline - uptime : 0); \
            result = blocker_block(blocker, remaining); \
            uptime = time_uptime(); \
            lock_acquire(lock); \
        } \
        result; \
    })

typedef struct blocker_entry
{
    list_entry_t entry;
    thread_t* thread;
    blocker_t* blocker;
} blocker_entry_t;

typedef struct blocker
{
    lock_t lock;
    list_t entries;
} blocker_t;

void blocker_init(blocker_t* blocker);

void blocker_deinit(blocker_t* blocker);

block_result_t blocker_block(blocker_t* blocker, nsec_t timeout);

block_result_t blocker_block_many(blocker_t** blockers, uint64_t amount, nsec_t timeout);

void blocker_unblock(blocker_t* blocker);

void waitsys_init(void);

void waitsys_update(trap_frame_t* trapFrame);

void waitsys_block(trap_frame_t* trapFrame);
