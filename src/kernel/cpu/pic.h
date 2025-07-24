#pragma once

#include <stdint.h>

// TODO: Replace with io apic

#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_COMMAND PIC1
#define PIC1_DATA (PIC1 + 1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA (PIC2 + 1)

#define PIC_EOI 0x20

void pic_init(void);

void pic_eoi(uint8_t irq);

void pic_set_mask(uint8_t irq);

void pic_clear_mask(uint8_t irq);
