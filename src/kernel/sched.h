#pragma once

#include "defs.h"
#include "lock.h"
#include "queue.h"
#include "thread.h"

#include <sys/list.h>

// Blocks untill condition is true, condition will be tested after every call to sched_unblock.
#define SCHED_BLOCK(blocker, condition) \
    ({ \
        block_result_t result = BLOCK_NORM; \
        while (!(condition) && result == BLOCK_NORM) \
        { \
            result = sched_block(blocker, NEVER); \
        } \
        result; \
    })

// Blocks untill condition is true, condition will be tested after every call to sched_unblock.
// Will also return after timeout is reached, timeout will be reached even if sched_unblock is never called.
#define SCHED_BLOCK_TIMEOUT(blocker, condition, timeout) \
    ({ \
        block_result_t result = BLOCK_NORM; \
        nsec_t uptime = time_uptime(); \
        nsec_t deadline = (timeout) == NEVER ? NEVER : (timeout) + uptime; \
        while (!(condition) && result == BLOCK_NORM) \
        { \
            if (deadline < uptime) \
            { \
                result = BLOCK_TIMEOUT; \
                break; \
            } \
            nsec_t remaining = deadline == NEVER ? NEVER : (deadline > uptime ? deadline - uptime : 0); \
            result = sched_block(blocker, remaining); \
            uptime = time_uptime(); \
        } \
        result; \
    })

// Blocks untill condition is true, condition will be tested after every call to sched_unblock.
// When condition is tested it will also acquire lock, and the macro will always return with lock still acquired.
#define SCHED_BLOCK_LOCK(blocker, lock, condition) \
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
            result = sched_block(blocker, NEVER); \
            lock_acquire(lock); \
        } \
        result; \
    })

// Blocks untill condition is true, condition will be tested after every call to sched_unblock.
// When condition is tested it will also acquire lock, and the macro will always return with lock still acquired.
// Will also return after timeout is reached, timeout will be reached even if sched_unblock is never called.
#define SCHED_BLOCK_LOCK_TIMEOUT(blocker, lock, condition, timeout) \
    ({ \
        block_result_t result = BLOCK_NORM; \
        nsec_t uptime = time_uptime(); \
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
            result = sched_block(blocker, remaining); \
            uptime = time_uptime(); \
            lock_acquire(lock); \
        } \
        result; \
    })

typedef struct
{
    queue_t queues[PRIORITY_LEVELS];
    list_t graveyard;
    thread_t* runThread;
} sched_context_t;

typedef struct blocker
{
    list_entry_t entry;
    list_t threads;
    lock_t lock;
} blocker_t;

void blocker_init(blocker_t* blocker);

void blocker_cleanup(blocker_t* blocker);

void sched_context_init(sched_context_t* context);

extern void sched_idle_loop(void);

void sched_init(void);

void sched_start(void);

block_result_t sched_sleep(nsec_t timeout);

block_result_t sched_block(blocker_t* blocker, nsec_t timeout);

void sched_unblock(blocker_t* blocker);

thread_t* sched_thread(void);

process_t* sched_process(void);

void sched_invoke(void);

void sched_yield(void);

NORETURN void sched_process_exit(uint64_t status);

NORETURN void sched_thread_exit(void);

void sched_push(thread_t* thread);

void sched_schedule(trap_frame_t* trapFrame);
