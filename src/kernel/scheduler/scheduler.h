#pragma once

#include "time/time.h"
#include "lock/lock.h"
#include "queue/queue.h"
#include "array/array.h"
#include "types/types.h"
#include "process/process.h"
#include "interrupt_frame/interrupt_frame.h"

#define SCHEDULER_TIME_SLICE (NANOSECONDS_PER_SECOND / 2)
#define SCHEDULER_TIMER_HZ 1024

typedef struct
{
    Queue* queues[THREAD_PRIORITY_LEVELS];
    Queue* killedThreads;
    Array* blockedThreads;
    Thread* runningThread;
} Scheduler;

extern void scheduler_idle_loop(void);

void scheduler_init(Scheduler* scheduler);

void scheduler_start(void);

void scheduler_cpu_start(void);

Thread* scheduler_thread(void);

Process* scheduler_process(void);

void scheduler_yield(void);

void scheduler_sleep(uint64_t nanoseconds);

void scheduler_block(Blocker blocker);

void scheduler_process_exit(uint64_t status);

void scheduler_thread_exit(void);

uint64_t scheduler_spawn(const char* path);

uint64_t scheduler_local_thread_amount(void);