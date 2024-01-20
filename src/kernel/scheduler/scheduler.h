#pragma once

#include "process/process.h"
#include "queue/queue.h"
#include "spin_lock/spin_lock.h"
#include "smp/smp.h"
#include "vector/vector.h"

#define TASK_STATE_RUNNING 0
#define TASK_STATE_EXPRESS 1
#define TASK_STATE_READY 2
#define TASK_STATE_BLOCKED 3

typedef struct
{
    Process* process;
    InterruptFrame* interruptFrame;

    uint8_t state;
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

    Queue* expressQueue;
    Queue* readyQueue;
    Task* runningTask;

    Vector* blockedTasks;

    uint64_t nextPreemption;

    SpinLock lock;
} Scheduler;

extern void scheduler_idle_loop();

void scheduler_init();

void scheduler_push(Process* process, InterruptFrame* interruptFrame);

void scheduler_acquire_all();

void scheduler_release_all();

Scheduler* scheduler_get_local();

void local_scheduler_tick(InterruptFrame* interruptFrame);

void local_scheduler_schedule(InterruptFrame* interruptFrame);

void local_scheduler_block(InterruptFrame* interruptFrame, Blocker blocker);

void local_scheduler_exit();

void local_scheduler_acquire();

void local_scheduler_release();

uint64_t local_scheduler_deadline();

Task* local_scheduler_running_task();
