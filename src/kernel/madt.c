#include "madt.h"

#include "log.h"

static madt_t* madt;

void madt_init(void)
{
    madt = (madt_t*)acpi_lookup("APIC");
    ASSERT_PANIC_MSG(madt != NULL, "Unable to find madt, hardware is not compatible");
}

madt_t* madt_get(void)
{
    return madt;
}

void* madt_lapic_address(void)
{
    return (void*)(uint64_t)madt->lapicAddress;
}
