#pragma once

#include <stdint.h>

#include "interrupt_frame/interrupt_frame.h"

enum 
{
    IRQ_PIT = 0,
    IRQ_KEYBOARD = 1,
    IRQ_CASCADE = 2,
    IRQ_COM2 = 3,
    IRQ_COM1 = 4,
    IRQ_LPT2 = 5,
    IRQ_FLOPPY = 6,
    IRQ_LPT1 = 7,
    IRQ_CMOS = 8,
    IRQ_FREE1 = 9,
    IRQ_FREE2 = 10,
    IRQ_FREE3 = 11,
    IRQ_PS2_MOUSE = 12,
    IRQ_FPU = 13,
    IRQ_PRIMARY_ATA_HARD_DISK = 14,
    IRQ_SECONDARY_ATA_HARD_DISK = 15
};

void interrupt_handler(InterruptFrame* interruptFrame);

void irq_handler(InterruptFrame* interruptFrame);

void exception_handler(InterruptFrame* interruptFrame);