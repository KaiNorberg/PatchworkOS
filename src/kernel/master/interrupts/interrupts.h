#pragma once

#include "idt/idt.h"
#include "interrupt_frame/interrupt_frame.h"

#define IRQ_BASE 0x20
#define IRQ_FAST_TIMER 0x0
#define IRQ_KEYBOARD 0x1
#define IRQ_CASCADE 0x2
#define IRQ_COM2 0x3
#define IRQ_COM1 0x4
#define IRQ_LPT2 0x5
#define IRQ_FLOPPY 0x6
#define IRQ_LPT1 0x7
#define IRQ_SLOW_TIMER 0x8
#define IRQ_FREE1 0x9
#define IRQ_FREE2 0xA
#define IRQ_FREE3 0xB
#define IRQ_PS2_MOUSE 0xC
#define IRQ_FPU 0xD
#define IRQ_PRIMARY_ATA_HARD_DISK 0xE
#define IRQ_SECONDARY_ATA_HARD_DISK 0xF
#define IRQ_AMOUNT 0x10

void master_idt_init();

void master_interrupt_handler(InterruptFrame const* interruptFrame);

void master_exception_handler(InterruptFrame const* interruptFrame);

void master_irq_handler(InterruptFrame const* interruptFrame);