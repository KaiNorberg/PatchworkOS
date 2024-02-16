#pragma once

#include "queue/queue.h"
#include "vector/vector.h"
#include "lock/lock.h"
#include "interrupt_frame/interrupt_frame.h"
#include "time/time.h"
#include "list/list.h"

#include "worker/process/process.h"

#define SCHEDULER_TIME_SLICE (NANOSECONDS_PER_SECOND / 2)

typedef struct
{
    Queue* queues[PROCESS_PRIORITY_LEVELS];
    Process* runningProcess;

    List* blockedProcesses;

    uint64_t nextPreemption;

    Lock lock;
} Scheduler;

typedef struct
{
    Process* process;
    Scheduler* scheduler;

    uint64_t timeout;
    uint8_t unblock;
} BlockedProcess;

extern void scheduler_idle_loop();

Scheduler* scheduler_new();

void scheduler_acquire(Scheduler* scheduler);

void scheduler_release(Scheduler* scheduler);

void scheduler_push(Scheduler* scheduler, Process* process);

void scheduler_exit(Scheduler* scheduler);

void scheduler_schedule(Scheduler* scheduler, InterruptFrame* interruptFrame);

BlockedProcess* scheduler_block(Scheduler* scheduler, InterruptFrame* interruptFrame, uint64_t timeout);

void scheduler_unblock(Scheduler* scheduler);

uint8_t scheduler_wants_to_schedule(Scheduler const* scheduler);

uint64_t scheduler_process_amount(Scheduler const* scheduler);