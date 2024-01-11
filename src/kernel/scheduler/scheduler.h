#pragma once

#include "process/process.h"
#include "queue/queue.h"
#include "spin_lock/spin_lock.h"

typedef struct
{
    Queue* readyQueue;
    Process* runningProcess;

    SpinLock lock;
} Scheduler;

extern void scheduler_yield_to_user_space(void* stackTop);

extern void scheduler_idle_loop();

void scheduler_init();

void scheduler_schedule(InterruptFrame* interruptFrame);

void scheduler_acquire();

void scheduler_release();

void scheduler_push(Process* process);