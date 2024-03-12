#pragma once

#include <stdint.h>

#include "time/time.h"
#include "lock/lock.h"
#include "list/list.h"
#include "queue/queue.h"
#include "process/process.h"
#include "interrupt_frame/interrupt_frame.h"

#define THREAD_STATE_ACTIVE 0
#define THREAD_STATE_KILLED 1

#define THREAD_PRIORITY_LEVELS 2
#define THREAD_PRIORITY_MIN 0
#define THREAD_PRIORITY_MAX (THREAD_PRIORITY_LEVELS - 1)

#define SCHEDULER_TIME_SLICE (NANOSECONDS_PER_SECOND / 2)
#define SCHEDULER_TIMER_HZ 1024

typedef struct
{
    Process* process;

    uint64_t id;

    void* kernelStackTop;
    void* kernelStackBottom;

    uint64_t timeEnd;
    uint64_t timeStart;

    InterruptFrame* interruptFrame;
    Status status;
    
    uint8_t state;
    uint8_t priority;
    uint8_t boost;
} Thread;

typedef struct
{
    uint64_t id;

    Queue* queues[THREAD_PRIORITY_LEVELS];
    Thread* runningThread;
} Scheduler;

extern void scheduler_idle_loop();

extern void scheduler_yield();

void scheduler_init();

void scheduler_cpu_start();

Thread* scheduler_thread();

Process* scheduler_process();

void scheduler_exit(Status status);

int64_t scheduler_spawn(const char* path);

void scheduler_schedule(InterruptFrame* interruptFrame);

//Temporary
uint64_t scheduler_local_thread_amount();