#pragma once

#include "defs.h"
#include "list.h"
#include "queue.h"
#include "process.h"

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

void sched_sleep(uint64_t nanoseconds);

void sched_block(Blocker blocker);

NORETURN void sched_process_exit(uint64_t status);

NORETURN void sched_thread_exit(void);

uint64_t sched_spawn(const char* path);

uint64_t sched_local_thread_amount(void);

void sched_schedule(TrapFrame* trapFrame);

void sched_push(Thread* thread, uint8_t boost);