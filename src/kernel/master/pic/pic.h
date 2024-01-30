#pragma once

#include <stdint.h>

#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_COMMAND PIC1
#define PIC1_DATA (PIC1+1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA (PIC2+1)

#define PIC_EOI 0x20

#define IRQ_BASE 0x20
#define IRQ_TIMER 0x0
#define IRQ_KEYBOARD 0x1
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
#define IRQ_PS2_MOUSE 0xC
#define IRQ_FPU 0xD
#define IRQ_PRIMARY_ATA_HARD_DISK 0xE
#define IRQ_SECONDARY_ATA_HARD_DISK 0xF
#define IRQ_AMOUNT 0x10

void pic_remap();

void pic_eoi(uint8_t irq);

void pic_set_mask(uint8_t irq);

void pic_clear_mask(uint8_t irq);