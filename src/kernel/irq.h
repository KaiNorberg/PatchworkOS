#pragma once

#include "defs.h"
#include "trap.h"

#define IRQ_PIT 0x0
#define IRQ_PS2_KBD 0x1
#define IRQ_CASCADE 0x2
#define IRQ_COM2 0x3
#define IRQ_COM1 0x4
#define IRQ_LPT2 0x5
#define IRQ_FLOPPY 0x6
#define IRQ_LPT1 0x7
#define IRQ_CMOS 0x8
#define IRQ_FREE1 0x9
#define IRQ_FREE2 0xA
#define IRQ_FREE3 0xB
#define IRQ_PS2_AUX 0xC
#define IRQ_FPU 0xD
#define IRQ_PRIMARY_ATA_HARD_DRIVE 0xE
#define IRQ_SECONDARY_ATA_HARD_DRIVE 0xF
#define IRQ_AMOUNT 0x10

#define IRQ_MAX_HANDLER 16

typedef void (*irq_handler_t)(uint8_t irq);

void irq_dispatch(trap_frame_t* trapFrame);

void irq_install(irq_handler_t handler, uint8_t irq);
