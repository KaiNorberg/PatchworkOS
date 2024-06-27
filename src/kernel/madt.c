#include "madt.h"

#include "log.h"

static madt_t* madt;

void madt_init(void)
{
    madt = (madt_t*)acpi_lookup("APIC");
    LOG_ASSERT(madt != NULL, "Unable to find madt, hardware is not compatible");
}

void* madt_lapic_address(void)
{
    return (void*)(uint64_t)madt->lapicAddress;
}

void* madt_first_record(uint8_t type)
{
    for (madt_header_t* record = madt->records; (uint64_t)record < (uint64_t)madt + madt->header.length;
         record = (madt_header_t*)((uint64_t)record + record->length))
    {
        if (record->type == type)
        {
            return record;
        }
    }

    return NULL;
}

void* madt_next_record(void* prev, uint8_t type)
{
    for (madt_header_t* record = (madt_header_t*)((uint64_t)prev + ((madt_header_t*)prev)->length);
         (uint64_t)record < (uint64_t)madt + madt->header.length; record = (madt_header_t*)((uint64_t)record + record->length))
    {
        if (record->type == type)
        {
            return record;
        }
    }

    return NULL;
}
