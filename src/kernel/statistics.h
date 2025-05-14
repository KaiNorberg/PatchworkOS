#pragma once

#include "lock.h"
#include "trap.h"

#include <time.h>

typedef struct cpu cpu_t;

typedef struct
{
    clock_t idleClocks;
    clock_t activeClocks;
    clock_t trapClocks;
    clock_t trapBegin;
    clock_t trapEnd;
    lock_t lock;
} statistics_cpu_ctx_t;

void statistics_cpu_ctx_init(statistics_cpu_ctx_t* ctx);

void statistics_init(void);

void statistics_trap_begin(trap_frame_t* trapFrame, cpu_t* cpu);

void statistics_trap_end(trap_frame_t* trapFrame, cpu_t* cpu);