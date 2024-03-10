#pragma once

#include <stdint.h>

#include "time/time.h"
#include "lock/lock.h"
#include "list/list.h"
#include "queue/queue.h"
#include "process/process.h"
#include "interrupt_frame/interrupt_frame.h"

#define THREAD_STATE_NONE 0
#define THREAD_STATE_RUNNING 1
#define THREAD_STATE_READY 2    
#define THREAD_STATE_BLOCKED 3

#define THREAD_PRIORITY_LEVELS 2
#define THREAD_PRIORITY_MIN 0
#define THREAD_PRIORITY_MAX (THREAD_PRIORITY_LEVELS - 1)

#define SCHEDULER_TIME_SLICE (NANOSECONDS_PER_SECOND / 2)

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
} Thread;

typedef struct
{
    Queue* queues[THREAD_PRIORITY_LEVELS];
    Thread* runningThread;
    
    Lock lock;
} Scheduler;

extern void scheduler_idle_loop();

extern void scheduler_yield();

void scheduler_init();

Thread* scheduler_thread();

Process* scheduler_process();

void scheduler_exit(Status status);

int64_t scheduler_spawn(const char* path);

void scheduler_schedule(InterruptFrame* interruptFrame);

//Temporary
uint64_t scheduler_local_thread_amount();