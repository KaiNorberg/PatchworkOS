#pragma once

#include "process/process.h"
#include "queue/queue.h"
#include "spin_lock/spin_lock.h"
#include "smp/smp.h"
#include "vector/vector.h"

#define TASK_STATE_NONE 0
#define TASK_STATE_RUNNING 1
#define TASK_STATE_READY 2
#define TASK_STATE_BLOCKED 3

#define TASK_PRIORITY_LEVELS 2
#define TASK_PRIORITY_MIN 0
#define TASK_PRIORITY_MAX (TASK_PRIORITY_LEVELS - 1)

#define SCHEDULER_TIME_SLICE (NANOSECONDS_PER_SECOND / 2)

#define SCHEDULER_BALANCING_PERIOD ((uint64_t)NANOSECONDS_PER_SECOND * 2)
#define SCHEDULER_BALANCING_ITERATIONS 2

typedef struct
{
    Process* process;
    InterruptFrame* interruptFrame;

    uint8_t state;
    uint8_t priority;
} Task;

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

    uint64_t nextPreemption;

    SpinLock lock;

    uint64_t ticks;
} Scheduler;

extern void scheduler_idle_loop();

void scheduler_init();

void scheduler_tick(InterruptFrame* interruptFrame);

void scheduler_balance(uint8_t priority);

void scheduler_acquire_all();

void scheduler_release_all();

void scheduler_emplace(Process* process, InterruptFrame* interruptFrame, uint8_t priority);

void scheduler_push(Task* task);

Scheduler* scheduler_get_local();

void local_scheduler_push(Process* process, InterruptFrame* interruptFrame, uint8_t priority);

void local_scheduler_tick(InterruptFrame* interruptFrame);

void local_scheduler_schedule(InterruptFrame* interruptFrame);

void local_scheduler_block(InterruptFrame* interruptFrame, Blocker blocker);

void local_scheduler_exit();

void local_scheduler_acquire();

void local_scheduler_release();

Task* local_scheduler_running_task();
