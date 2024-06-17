#include "madt.h"

#include "debug.h"

static Madt* madt;

void madt_init(void)
{
    madt = (Madt*)rsdt_lookup("APIC");
    DEBUG_ASSERT(madt != NULL, "lookup fail");
}

void* madt_local_apic_address(void)
{
    return (void*)(uint64_t)madt->localApicAddress;
}

void* madt_first_record(uint8_t type)
{
    for (RecordHeader* record = madt->records; (uint64_t)record < (uint64_t)madt + madt->header.length;
         record = (RecordHeader*)((uint64_t)record + record->length))
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
    for (RecordHeader* record = (RecordHeader*)((uint64_t)prev + ((RecordHeader*)prev)->length);
         (uint64_t)record < (uint64_t)madt + madt->header.length; record = (RecordHeader*)((uint64_t)record + record->length))
    {
        if (record->type == type)
        {
            return record;
        }
    }

    return NULL;
}
