#pragma once

#include "interrupt_frame/interrupt_frame.h"

typedef struct
{
    uint64_t enabled;
    uint64_t depth;
    uint64_t cliAmount;
} InterruptState;

uint64_t interrupt_depth(void);

void interrupts_disable(void);

void interrupts_enable(void);

void interrupt_handler(InterruptFrame* interruptFrame);