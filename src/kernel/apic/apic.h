#pragma once

#include "rsdt/rsdt.h"

#define LOCAL_APIC_REGISTER_ID 0x020
#define LOCAL_APIC_REGISTER_EOI 0x0B0
#define LOCAL_APIC_REGISTER_SPURIOUS 0x0F0
#define LOCAL_APIC_REGISTER_ICR0 0x300
#define LOCAL_APIC_REGISTER_ICR1 0x310

#define LOCAL_APIC_ID_OFFSET 24

void apic_init();

uint32_t local_apic_current_cpu();

void local_apic_init();

void local_apic_write(uint32_t reg, uint32_t value);

uint32_t local_apic_read(uint32_t reg);

void local_apic_send_init(uint32_t localApicId);

void local_apic_send_sipi(uint32_t localApicId, uint32_t page);

void local_apic_send_ipi(uint32_t localApicId, uint64_t vector);

void local_apic_eoi();