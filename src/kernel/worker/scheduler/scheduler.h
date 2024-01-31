#pragma once

#include "queue/queue.h"
#include "process/process.h"
#include "vector/vector.h"
#include "lock/lock.h"
#include "interrupt_frame/interrupt_frame.h"
#include "time/time.h"

#define SCHEDULER_TIME_SLICE (NANOSECONDS_PER_SECOND / 2)

typedef struct
{
    uint64_t timeout;
} Blocker;

typedef struct
{
    Task* task;
    Blocker blocker;
} BlockedTask;

typedef struct
{
    Queue* queues[TASK_PRIORITY_LEVELS];
    Task* runningTask;

    Vector* blockedTasks;

    uint64_t nextPreemption;

    Lock lock;
} Scheduler;

extern void scheduler_idle_loop();

Scheduler* scheduler_new();

void scheduler_acquire(Scheduler* scheduler);

void scheduler_release(Scheduler* scheduler);

void scheduler_push(Scheduler* scheduler, Task* task);

void scheduler_exit(Scheduler* scheduler);

void scheduler_schedule(Scheduler* scheduler, InterruptFrame* interruptFrame);

void scheduler_block(Scheduler* scheduler, InterruptFrame* interruptFrame, Blocker blocker);

void scheduler_unblock(Scheduler* scheduler);

uint8_t scheduler_wants_to_schedule(Scheduler const* scheduler);

uint64_t scheduler_task_amount(Scheduler const* scheduler);