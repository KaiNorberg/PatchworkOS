#pragma once

#include "defs.h"
#include "rsdt.h"

#define MADT_LAPIC 0

#define MADT_LAPIC_INITABLE (1 << 0)

#define MADT_FOR_EACH(record, type, records) \
    for ((record) = (typeof(record))(records); (uint64_t)(record) < (uint64_t)madt + madt->header.length; \
         record = (madt_header_t*)((uint64_t)record + record->length))

typedef struct PACKED
{
    uint8_t type;
    uint8_t length;
} madt_header_t;

typedef struct PACKED
{
    madt_header_t header;

    uint8_t cpuId;
    uint8_t localApicId;
    uint32_t flags;
} madt_lapic_t;

typedef struct PACKED
{
    sdt_t header;
    uint32_t localApicAddress;
    uint32_t flags;
    madt_header_t records[];
} madt_t;

void madt_init(void);

void* madt_lapic_address(void);

void* madt_first_record(uint8_t type);

void* madt_next_record(void* prev, uint8_t type);
