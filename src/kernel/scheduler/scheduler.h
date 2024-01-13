#pragma once

#include "process/process.h"
#include "queue/queue.h"
#include "spin_lock/spin_lock.h"

#define TASK_STATE_RUNNING 0
#define TASK_STATE_READY 1

typedef struct
{
    Process* process;
    InterruptFrame* interruptFrame;

    uint8_t state;
} Task;

typedef struct
{
    Queue* readyQueue;
    Task* runningTask;

    uint64_t nextPreemption;

    SpinLock lock;
} Scheduler;

extern void scheduler_idle_loop();

void scheduler_init();

void scheduler_schedule(InterruptFrame* interruptFrame);

void scheduler_push(Process* process, InterruptFrame* interruptFrame);

void scheduler_exit();

uint64_t scheduler_deadline();

Task* scheduler_running_task();

Scheduler* scheduler_get();