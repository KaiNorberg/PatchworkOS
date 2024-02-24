#include "madt.h"

#include "tty/tty.h"

Madt* madt;

void madt_init()
{    
    tty_start_message("MADT initializing");

    madt = (Madt*)rsdt_lookup("APIC");    
    tty_assert(madt != 0, "Hardware is incompatible, unable to find MADT");

    tty_end_message(TTY_MESSAGE_OK);
} 

void* madt_first_record(uint8_t type)
{
    for (RecordHeader* record = madt->records; (uint64_t)record < (uint64_t)madt + madt->header.length; record = (RecordHeader*)((uint64_t)record + record->length))
    {
        if (record->type == type)
        {
            return record;
        }
    }

    return 0;
}

void* madt_next_record(void* prev, uint8_t type)
{   
    RecordHeader* record = (RecordHeader*)((uint64_t)prev + ((RecordHeader*)prev)->length);
    for (;(uint64_t)record < (uint64_t)madt + madt->header.length; record = (RecordHeader*)((uint64_t)record + ((RecordHeader*)record)->length))
    {
        if (((RecordHeader*)record)->type == type)
        {
            return record;
        }
    }

    return 0;
}

void* madt_local_apic_address()
{
    return (void*)(uint64_t)madt->localApicAddress;
}