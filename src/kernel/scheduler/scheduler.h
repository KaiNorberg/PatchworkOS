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
    Queue* graveyard;
    Process* runningProcess;
} Scheduler;

extern void scheduler_idle_loop(void);

extern void scheduler_yield(void);

void scheduler_init(void);

void scheduler_cpu_start(void);

Scheduler* scheduler_get(uint64_t id);

Scheduler* scheduler_local(void);

void scheduler_put(void);

Process* scheduler_process(void);

void scheduler_exit(Status status);

int64_t scheduler_spawn(const char* path);

//Temporary
uint64_t scheduler_local_process_amount(void);