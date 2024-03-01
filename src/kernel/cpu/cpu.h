#pragma once

#include <stdint.h>

#include "tss/tss.h"
#include "ipi/ipi.h"
#include "interrupt_frame/interrupt_frame.h"

#define CPU_MAX_AMOUNT 256

typedef struct
{
    uint8_t present;
    
    uint8_t id; 
    uint8_t localApicId;    
    
    Ipi ipi;

    Tss* tss;
    InterruptFrame* interruptFrame;
} Cpu;

void cpu_entry();

uint8_t cpu_init(Cpu* cpu, uint8_t id, uint8_t localApicId);

void cpu_begin_interrupt(InterruptFrame* interruptFrame);

void cpu_end_interrupt();