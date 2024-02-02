#pragma once

#include "rsdt/rsdt.h"

#define APIC_TIMER_HZ 1000

#define LOCAL_APIC_MSR_ENABLE 0x800

#define LOCAL_APIC_REG_ID 0x020
#define LOCAL_APIC_REG_EOI 0x0B0
#define LOCAL_APIC_REG_SPURIOUS 0x0F0
#define LOCAL_APIC_REG_ICR0 0x300
#define LOCAL_APIC_REG_ICR1 0x310

#define LOCAL_APIC_REG_LVT_TIMER 0x320
#define LOCAL_APIC_REG_TIMER_INITIAL_COUNT 0x380
#define LOCAL_APIC_REG_TIMER_CURRENT_COUNT 0x390
#define LOCAL_APIC_REG_TIMER_DIVIDER 0x3E0

#define APIC_TIMER_MASKED 0x10000
#define APIC_TIMER_PERIODIC 0x20000

#define LOCAL_APIC_ID_OFFSET 24

void apic_init();

void apic_timer_init(uint8_t vector, uint64_t hz);

void local_apic_init();

uint8_t local_apic_id();

void local_apic_write(uint32_t reg, uint32_t value);

uint32_t local_apic_read(uint32_t reg);

void local_apic_send_init(uint32_t apicId);

void local_apic_send_sipi(uint32_t apicId, uint32_t page);

void local_apic_send_ipi(uint32_t apicId, uint8_t vector);

void local_apic_eoi();