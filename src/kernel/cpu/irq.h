#pragma once

#include "trap.h"

#include <stdbool.h>

typedef enum
{
    IRQ_PIT = 0x0,
    IRQ_PS2_FIRST_DEVICE = 0x1,
    IRQ_CASCADE = 0x2,
    IRQ_COM2 = 0x3,
    IRQ_COM1 = 0x4,
    IRQ_LPT2 = 0x5,
    IRQ_FLOPPY = 0x6,
    IRQ_LPT1 = 0x7,
    IRQ_CMOS = 0x8,
    IRQ_FREE1 = 0x9,
    IRQ_FREE2 = 0xA,
    IRQ_FREE3 = 0xB,
    IRQ_PS2_SECOND_DEVICE = 0xC,
    IRQ_FPU = 0xD,
    IRQ_PRIMARY_ATA_HARD_DRIVE = 0xE,
    IRQ_SECONDARY_ATA_HARD_DRIVE = 0xF,
    IRQ_AMOUNT = 0x10
} irq_t;

#define IRQ_MAX_CALLBACK 16

typedef void (*irq_callback_t)(irq_t irq);

typedef struct
{
    irq_callback_t callbacks[IRQ_MAX_CALLBACK];
    uint32_t callbackAmount;
    bool redirected;
} irq_handler_t;

void irq_dispatch(trap_frame_t* trapFrame);

void irq_install(irq_t irq, irq_callback_t callback);
void irq_uninstall(irq_t irq, irq_callback_t callback);
