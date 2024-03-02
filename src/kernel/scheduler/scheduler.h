#pragma once

#include "process/process.h"

typedef struct
{
    Process* process;
    uint64_t* threadCount;

    InterruptFrame* interruptFrame;
} Thread;

typedef struct
{

} Scheduler;

void scheduler_init();

void scheduler_spawn(const char* path);

void scheduler_schedule();