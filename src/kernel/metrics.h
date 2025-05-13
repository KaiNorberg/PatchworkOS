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
} metrics_cpu_ctx_t;

void metrics_cpu_ctx_init(metrics_cpu_ctx_t* ctx);

void metrics_init(void);

void metrics_trap_begin(trap_frame_t* trapFrame, cpu_t* cpu);

void metrics_trap_end(trap_frame_t* trapFrame, cpu_t* cpu);