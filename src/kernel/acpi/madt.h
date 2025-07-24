#pragma once

#include "acpi.h"

#define MADT_LAPIC 0

#define MADT_LAPIC_INITABLE (1 << 0)

#define MADT_FOR_EACH(madt, record) \
    for (record = (typeof(record))madt->records; \
        (uint8_t*)record < (uint8_t*)madt + madt->header.length && \
        (uint8_t*)record + sizeof(madt_header_t) <= (uint8_t*)madt + madt->header.length && \
        (uint8_t*)record + record->header.length <= (uint8_t*)madt + madt->header.length; \
        record = (typeof(record))((uint8_t*)record + record->header.length))

typedef struct PACKED
{
    uint8_t type;
    uint8_t length;
} madt_header_t;

typedef struct PACKED
{
    madt_header_t header;
    uint8_t cpuId;
    uint8_t lapicId;
    uint32_t flags;
} madt_lapic_t;

typedef struct PACKED
{
    sdt_t header;
    uint32_t lapicAddress;
    uint32_t flags;
    madt_header_t records[];
} madt_t;

void madt_init(void);

madt_t* madt_get(void);

void* madt_lapic_address(void);
