#pragma once

#include "cpu/trap.h"
#include "sync/lock.h"

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

void statistics_trap_begin(trap_frame_t* trapFrame, cpu_t* self);

void statistics_trap_end(trap_frame_t* trapFrame, cpu_t* self);