#pragma once

#include <stdint.h>

#include "time/time.h"
#include "lock/lock.h"
#include "list/list.h"
#include "queue/queue.h"
#include "process/process.h"
#include "interrupt_frame/interrupt_frame.h"

#define SCHEDULER_TIME_SLICE (NANOSECONDS_PER_SECOND / 2)
#define SCHEDULER_TIMER_HZ 1024

typedef struct
{
    uint64_t id;

    Queue* queues[PROCESS_PRIORITY_LEVELS];
    Process* runningProcess;
} Scheduler;

extern void scheduler_idle_loop();

extern void scheduler_yield();

void scheduler_init();

void scheduler_cpu_start();

Scheduler* scheduler_get(uint64_t id);

Scheduler* scheduler_local();

void scheduler_put();

Process* scheduler_process();

void scheduler_exit(Status status);

int64_t scheduler_spawn(const char* path);

//Temporary
uint64_t scheduler_local_process_amount();