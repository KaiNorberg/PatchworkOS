#pragma once

#include <stdint.h>

#include "rsdt/rsdt.h"

#define MADT_RECORD_TYPE_LOCAL_APIC 0

#define LOCAL_APIC_RECORD_FLAG_ENABLEABLE (1 << 0)

#define LOCAL_APIC_RECORD_GET_FLAG(record, flag) ((record->flags & flag) != 0)

typedef struct __attribute__((packed))
{
    uint8_t type;
    uint8_t length;
} RecordHeader;

typedef struct __attribute__((packed))
{
    RecordHeader header;

    uint8_t cpuId;
    uint8_t localApicId;
    uint32_t flags;
} LocalApicRecord;

typedef struct __attribute__((packed))
{
    SdtHeader header;
    uint32_t localApicAddress;
    uint32_t flags;
    RecordHeader records[];
} Madt;

void madt_init(void);

void* madt_first_record(uint8_t type);

void* madt_next_record(void* prev, uint8_t type);

void* madt_local_apic_address(void);