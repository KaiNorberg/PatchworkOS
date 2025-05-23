#include "madt.h"

#include "utils/log.h"

#include <assert.h>

static madt_t* madt;

void madt_init(void)
{
    madt = (madt_t*)acpi_lookup("APIC");
    assert(madt != NULL && "Unable to find madt, hardware is not compatible");
}

madt_t* madt_get(void)
{
    return madt;
}

void* madt_lapic_address(void)
{
    return (void*)(uint64_t)madt->lapicAddress;
}
