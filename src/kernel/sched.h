#pragma once

#include "defs.h"
#include "process.h"
#include "queue.h"
#include "time.h"

#include <sys/list.h>

#define SCHED_WAIT_NORMAL 0
#define SCHED_WAIT_TIMEOUT 1

#define SCHED_WAIT(condition, timeout) \
    ({ \
        nsec_t deadline = (timeout) == NEVER ? NEVER : (timeout) + time_uptime(); \
        uint8_t result = SCHED_WAIT_NORMAL; \
        while (!(condition)) \
        { \
            if (deadline < time_uptime()) \
            { \
                result = SCHED_WAIT_TIMEOUT; \
                break; \
            } \
            sched_pause(); \
        } \
        result; \
    })

typedef struct
{
    queue_t queues[THREAD_PRIORITY_LEVELS];
    list_t graveyard;
    thread_t* runningThread;
} sched_context_t;

void sched_context_init(sched_context_t* context);

extern void sched_idle_loop(void);

void sched_init(void);

void sched_start(void);

void sched_cpu_start(void);

thread_t* sched_thread(void);

process_t* sched_process(void);

void sched_yield(void);

// Yields the current thread's remaining time slice.
// If no other threads are ready, the CPU will idle until the next call to sched_schedule().
void sched_pause(void);

NORETURN void sched_process_exit(uint64_t status);

NORETURN void sched_thread_exit(void);

pid_t sched_spawn(const char* path, uint8_t priority);

tid_t sched_thread_spawn(void* entry, uint8_t priority);

uint64_t sched_local_thread_amount(void);

void sched_schedule(trap_frame_t* trapFrame);
