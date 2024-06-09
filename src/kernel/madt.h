#pragma once

#include "defs.h"
#include "rsdt.h"

#define MADT_RECORD_TYPE_LOCAL_APIC 0

#define LOCAL_APIC_RECORD_FLAG_ENABLEABLE (1 << 0)

#define LOCAL_APIC_RECORD_GET_FLAG(record, flag) ((record->flags & flag) != 0)

#define MADT_FOR_EACH(record, type, records) \
    for ((record) = (typeof(record))(records); (uint64_t)(record) < (uint64_t)madt + madt->header.length; \
         record = (RecordHeader*)((uint64_t)record + record->length))

typedef struct PACKED
{
    uint8_t type;
    uint8_t length;
} RecordHeader;

typedef struct PACKED
{
    RecordHeader header;

    uint8_t cpuId;
    uint8_t localApicId;
    uint32_t flags;
} LocalApicRecord;

typedef struct PACKED
{
    SdtHeader header;
    uint32_t localApicAddress;
    uint32_t flags;
    RecordHeader records[];
} Madt;

void madt_init(void);

void* madt_local_apic_address(void);

void* madt_first_record(uint8_t type);

void* madt_next_record(void* prev, uint8_t type);
