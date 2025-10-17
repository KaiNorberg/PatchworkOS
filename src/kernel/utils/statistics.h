#pragma once

#include "cpu/interrupt.h"
#include "sync/lock.h"

#include <time.h>

typedef struct cpu cpu_t;

typedef struct
{
    clock_t idleClocks;
    clock_t activeClocks;
    clock_t interruptClocks;
    clock_t interruptBegin;
    clock_t interruptEnd;
    lock_t lock;
} statistics_cpu_ctx_t;

void statistics_cpu_ctx_init(statistics_cpu_ctx_t* ctx);

void statistics_init(void);

void statistics_interrupt_begin(interrupt_frame_t* frame, cpu_t* self);

void statistics_interrupt_end(interrupt_frame_t* frame, cpu_t* self);
