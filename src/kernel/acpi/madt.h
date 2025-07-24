#pragma once

#include "acpi.h"

#define MADT_LAPIC 0
#define MADT_IOAPIC 1
#define MADT_INTERRUPT_OVERRIDE 2
#define MADT_NMI_SOURCE 3
#define MADT_LAPIC_NMI 4
#define MADT_LAPIC_ADDRESS_OVERRIDE 5

#define MADT_LAPIC_ENABLED (1 << 0)
#define MADT_LAPIC_ONLINE_CAPABLE (1 << 1)

#define MADT_FOR_EACH(madt, record) \
    for (record = (typeof(record))madt->records; (uint8_t*)record < (uint8_t*)madt + madt->header.length && \
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
    uint8_t id;
    uint32_t flags;
} madt_lapic_t;

typedef struct PACKED
{
    madt_header_t header;
    uint8_t id;
    uint8_t reserved;
    uint32_t address;
    uint32_t gsiBase;
} madt_ioapic_t;

typedef struct PACKED
{
    sdt_t header;
    uint32_t lapicAddress;
    uint32_t flags;
    madt_header_t records[];
} madt_t;

void madt_init(void);

madt_t* madt_get(void) PURE_FUNC;

void* madt_lapic_address(void);
