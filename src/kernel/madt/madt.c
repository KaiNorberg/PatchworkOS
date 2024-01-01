#include "madt.h"

#include "tty/tty.h"

Madt* madt;

void madt_init()
{    
    tty_start_message("MADT initializing");

    madt = (Madt*)rsdt_lookup("APIC");
    if (madt == 0)
    {
        tty_print("Hardware is incompatible, unable to find MADT");
        tty_end_message(TTY_MESSAGE_ER);
    }

    tty_end_message(TTY_MESSAGE_OK);
}

MadtRecord* madt_first_record(uint8_t type)
{
    for (MadtRecord* record = madt->records; (uint64_t)record < (uint64_t)madt + madt->header.length; record = (MadtRecord*)((uint64_t)record + record->length))
    {
        if (record->type == type)
        {
            return record;
        }
    }

    return 0;
}

MadtRecord* madt_next_record(MadtRecord* record, uint8_t type)
{        
    record = (MadtRecord*)((uint64_t)record + record->length);
    for (;(uint64_t)record < (uint64_t)madt + madt->header.length; record = (MadtRecord*)((uint64_t)record + record->length))
    {
        if (record->type == type)
        {
            return record;
        }
    }

    return 0;
}

uint64_t madt_local_apic_address()
{
    return madt->localApicAddress;
}

uint32_t madt_flags()
{
    return madt->flags;
}