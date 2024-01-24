#pragma once

#include "process/process.h"
#include "queue/queue.h"
#include "spin_lock/spin_lock.h"
#include "smp/smp.h"
#include "vector/vector.h"

#define SCHEDULER_TIME_SLICE (NANOSECONDS_PER_SECOND / 2)

#define SCHEDULER_BALANCING_PERIOD ((uint64_t)NANOSECONDS_PER_SECOND * 2)
#define SCHEDULER_BALANCING_ITERATIONS 2

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
    Cpu* cpu;

    Queue* queues[TASK_PRIORITY_LEVELS];
    Task* runningTask;

    Vector* blockedTasks;

    uint64_t nextPreemption;

    SpinLock lock;
} Scheduler;

extern void scheduler_idle_loop();

void scheduler_init();

void scheduler_acquire_all();

void scheduler_release_all();

void scheduler_push(Task* task);

Scheduler* scheduler_get_local();

Scheduler* scheduler_get(uint8_t cpuId);

void local_scheduler_push(Task* task);

void local_scheduler_tick(InterruptFrame* interruptFrame);

void local_scheduler_schedule(InterruptFrame* interruptFrame);

void local_scheduler_block(InterruptFrame* interruptFrame, Blocker blocker);

void local_scheduler_exit();

void local_scheduler_acquire();

void local_scheduler_release();

uint64_t local_scheduler_task_amount();

Task* local_scheduler_running_task();
