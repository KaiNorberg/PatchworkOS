#pragma once

#include "rsdt/rsdt.h"

#define MADT_RECORD_TYPE_LAPIC 0

#define MADT_LAPIC_RECORD_IS_ENABLEABLE(record) ((record->flags & 1) != 0) 

#define LAPIC_REGISTER_ID 0x020
#define LAPIC_REGISTER_ICR0 0x300
#define LAPIC_REGISTER_ICR1 0x310

typedef struct __attribute__((packed))
{
    uint8_t type;
    uint8_t length;
} MadtRecord;

typedef struct __attribute__((packed))
{
    MadtRecord record;

    uint8_t cpuId;
    uint8_t lapicId;
    uint32_t flags;
} MadtLapicRecord;

typedef struct __attribute__((packed)) 
{
    SdtHeader header;
    uint32_t lapicAddress;
    uint32_t flags; 

    MadtRecord records[];
} Madt;

void apic_init();

uint32_t lapic_current_cpu();

void lapic_write(uint32_t reg, uint32_t value);

uint32_t lapic_read(uint32_t reg);

void lapic_send_init(uint32_t cpu);

void lapic_send_sipi(uint32_t cpu, uint32_t page);