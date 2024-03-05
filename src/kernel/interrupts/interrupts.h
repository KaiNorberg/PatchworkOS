#pragma once

#include "page_directory/page_directory.h"
#include "interrupt_frame/interrupt_frame.h"

typedef struct
{
    uint64_t enabled;
    uint64_t depth;
    uint64_t cliAmount;
} InterruptState;

void interrupts_disable();

void interrupts_enable();

uint64_t interrupt_depth();

void interrupt_handler(InterruptFrame* interruptFrame);