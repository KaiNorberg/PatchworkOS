#include "madt.h"

#include "log/log.h"
#include "log/panic.h"

#include <assert.h>

static madt_t* madt;

static bool madt_is_record_valid(madt_header_t* record, uint64_t remainingBytes)
{
    if (record->length < sizeof(madt_header_t))
    {
        LOG_ERR("record to small %u\n", record->length);
        return false;
    }

    if (record->length > remainingBytes)
    {
        LOG_ERR("record header exceeds table bounds\n");
        return false;
    }

    return true;
}

static bool madt_is_valid(madt_t* madt)
{
    if (madt->header.length < sizeof(madt_t))
    {
        LOG_ERR("table too small\n");
        return false;
    }

    uint64_t offset = 0;
    uint64_t recordsSize = madt->header.length - sizeof(madt_t);
    madt_header_t* record = madt->records;
    while (offset < recordsSize)
    {
        if (!madt_is_record_valid(record, recordsSize - offset))
        {
            return false;
        }

        offset += record->length;
        record = (madt_header_t*)((uintptr_t)record + record->length);
    }

    return true;
}

void madt_init(void)
{
    madt = (madt_t*)acpi_lookup("APIC");
    if (madt == NULL)
    {
        panic(NULL, "Unable to find madt, hardware is not compatible");
    }

    if (!madt_is_valid(madt))
    {
        panic(NULL, "madt is not valid");
    }

    LOG_INFO("madt found with flags 0x%08x and local apic address 0x%08x\n", madt->flags, madt->lapicAddress);
}

madt_t* madt_get(void)
{
    return madt;
}

void* madt_lapic_address(void)
{
    return (void*)(uint64_t)madt->lapicAddress;
}
