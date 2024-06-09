#pragma once

#include "defs.h"
#include "list.h"
#include "process.h"
#include "queue.h"
#include "time.h"

// clang-format off
#define SCHED_WAIT(condition, timeout) \
({ \
    nsec_t deadline = (timeout) == UINT64_MAX ? UINT64_MAX : (timeout) + time_uptime(); \
    while (!(condition) && deadline > time_uptime()) \
    { \
        sched_pause(); \
    } \
    0; \
})
// clang-format on

typedef struct
{
    Queue queues[THREAD_PRIORITY_LEVELS];
    List killedThreads;
    List blockedThreads;
    Thread* runningThread;
} Scheduler;

extern void sched_idle_loop(void);

void scheduler_init(Scheduler* scheduler);

void sched_start(void);

void sched_cpu_start(void);

Thread* sched_thread(void);

Process* sched_process(void);

void sched_yield(void);

// Yields the current thread's remaining time slice.
// If no other threads are ready, the CPU will idle until the next call to sched_schedule().
void sched_pause(void);

NORETURN void sched_process_exit(uint64_t status);

NORETURN void sched_thread_exit(void);

pid_t sched_spawn(const char* path);

tid_t sched_thread_spawn(void* entry, uint8_t priority);

uint64_t sched_local_thread_amount(void);

void sched_schedule(TrapFrame* trapFrame);