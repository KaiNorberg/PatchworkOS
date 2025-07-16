#include "madt.h"

#include "log/log.h"
#include "log/panic.h"

#include <assert.h>

static madt_t* madt;

void madt_init(void)
{
    madt = (madt_t*)acpi_lookup("APIC");
    if (madt == NULL)
    {
        panic(NULL, "Unable to find madt, hardware is not compatible");
    }
}

madt_t* madt_get(void)
{
    return madt;
}

void* madt_lapic_address(void)
{
    return (void*)(uint64_t)madt->lapicAddress;
}
