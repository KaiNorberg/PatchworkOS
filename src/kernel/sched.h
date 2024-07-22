#pragma once

#include "defs.h"
#include "lock.h"
#include "process.h"
#include "queue.h"

#include <sys/list.h>

// Blocks untill condition is true, condition will be tested after every call to sched_unblock.
#define SCHED_BLOCK(blocker, condition) \
    ({ \
        block_result_t result = BLOCK_NORM; \
        sched_block_begin(blocker); \
        while (!(condition) && result == BLOCK_NORM) \
        { \
            result = sched_block_do(blocker, NEVER); \
        } \
        sched_block_end(blocker); \
        result; \
    })

// Blocks untill condition is true, condition will be tested after every call to sched_unblock.
// Will also return after timeout is reached, timeout will be reached even if sched_unblock is never called.
#define SCHED_BLOCK_TIMEOUT(blocker, condition, timeout) \
    ({ \
        block_result_t result = BLOCK_NORM; \
        nsec_t uptime = time_uptime(); \
        nsec_t deadline = (timeout) == NEVER ? NEVER : (timeout) + uptime; \
        sched_block_begin(blocker); \
        while (!(condition) && result == BLOCK_NORM && deadline > uptime) \
        { \
            nsec_t remaining = deadline == NEVER ? NEVER : (deadline > uptime ? deadline - uptime : 0); \
            result = sched_block_do(blocker, remaining); \
            uptime = time_uptime(); \
        } \
        sched_block_end(blocker); \
        result; \
    })

// Blocks untill condition is true, condition will be tested after every call to sched_unblock.
// When condition is tested it will also acquire lock, and the macro will always return with lock still acquired.
#define SCHED_BLOCK_LOCK(blocker, lock, condition) \
    ({ \
        block_result_t result = BLOCK_NORM; \
        sched_block_begin(blocker); \
        while (result == BLOCK_NORM) \
        { \
            lock_acquire(lock); \
            if (condition) \
            { \
                break; \
            } \
            lock_release(lock); \
            result = sched_block_do(blocker, NEVER); \
        } \
        sched_block_end(blocker); \
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
        sched_block_begin(blocker); \
        while (result == BLOCK_NORM && deadline > uptime) \
        { \
            lock_acquire(lock); \
            if (condition) \
            { \
                break; \
            } \
            lock_release(lock); \
            nsec_t remaining = deadline == NEVER ? NEVER : (deadline > uptime ? deadline - uptime : 0); \
            result = sched_block_do(blocker, remaining); \
            uptime = time_uptime(); \
        } \
        sched_block_end(blocker); \
        result; \
    })

typedef struct
{
    queue_t queues[THREAD_PRIORITY_LEVELS];
    list_t graveyard;
    thread_t* runThread;
} sched_context_t;

typedef struct blocker
{
    list_entry_t base;
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

void sched_block_begin(blocker_t* blocker);

block_result_t sched_block_do(blocker_t* blocker, nsec_t timeout);

void sched_block_end(blocker_t* blocker);

void sched_unblock(blocker_t* blocker);

thread_t* sched_thread(void);

process_t* sched_process(void);

void sched_invoke(void);

void sched_yield(void);

NORETURN void sched_process_exit(uint64_t status);

NORETURN void sched_thread_exit(void);

pid_t sched_spawn(const char* path, uint8_t priority);

tid_t sched_thread_spawn(void* entry, uint8_t priority);

void sched_schedule(trap_frame_t* trapFrame);
