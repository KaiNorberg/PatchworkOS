#pragma once

#include "defs.h"

#define APIC_TIMER_MASKED 0x10000
#define APIC_TIMER_PERIODIC 0x20000

#define LAPIC_MSR_ENABLE 0x800

#define LAPIC_REG_ID 0x020
#define LAPIC_REG_EOI 0x0B0
#define LAPIC_REG_SPURIOUS 0x0F0
#define LAPIC_REG_ICR0 0x300
#define LAPIC_REG_ICR1 0x310

#define LAPIC_REG_LVT_TIMER 0x320
#define LAPIC_REG_TIMER_INITIAL_COUNT 0x380
#define LAPIC_REG_TIMER_CURRENT_COUNT 0x390
#define LAPIC_REG_TIMER_DIVIDER 0x3E0

#define LAPIC_ID_OFFSET 24

void apic_init(void);

void apic_timer_init(uint8_t vector, uint64_t hz);

void lapic_init(void);

uint8_t lapic_id(void);

void lapic_write(uint32_t reg, uint32_t value);

uint32_t lapic_read(uint32_t reg);

void lapic_send_init(uint32_t apicId);

void lapic_send_sipi(uint32_t apicId, uint32_t page);

void lapic_send_ipi(uint32_t apicId, uint8_t vector);

void lapic_eoi(void);
