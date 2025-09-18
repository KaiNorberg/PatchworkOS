#include "hpet.h"

#include "acpi/tables.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/vmm.h"
#include "utils/utils.h"

#include <assert.h>

static hpet_t* hpet;
static uintptr_t address;
static uint64_t period; // Period in femtoseconds

void hpet_init(void)
{
    hpet = (hpet_t*)acpi_tables_lookup("HPET", 0);
    if (hpet == NULL)
    {
        panic(NULL, "Unable to find hpet, hardware is not compatible");
    }

    address = (uintptr_t)vmm_kernel_map(NULL, (void*)hpet->address, 1, PML_WRITE);
    if (address == (uintptr_t)NULL)
    {
        panic(NULL, "Unable to map hpet");
    }
    period = hpet_read(HPET_GENERAL_CAPABILITIES) >> HPET_COUNTER_CLOCK_OFFSET;

    LOG_INFO("hpet at phys=0x%016lx virt=0x%016lx period=%lufs creatorID=%llu\n", hpet->address, address, period / 1000000ULL, hpet->header.creatorID);

    hpet_reset_counter();
}

uint64_t hpet_nanoseconds_per_tick(void)
{
    return period / 1000000ULL;
}

uint64_t hpet_read_counter(void)
{
    return hpet_read(HPET_MAIN_COUNTER_VALUE);
}

void hpet_reset_counter(void)
{
    hpet_write(HPET_GENERAL_CONFIG, HPET_CFG_DISABLE);
    hpet_write(HPET_MAIN_COUNTER_VALUE, 0);
    hpet_write(HPET_GENERAL_CONFIG, HPET_CFG_ENABLE);
}

void hpet_write(uint64_t reg, uint64_t value)
{
    WRITE_64(address + reg, value);
}

uint64_t hpet_read(uint64_t reg)
{
    return READ_64(address + reg);
}

void hpet_wait(clock_t nanoseconds)
{
    if (nanoseconds == 0)
    {
        return;
    }

    uint64_t ticks = (nanoseconds * 1000000) / period;
    uint64_t start = hpet_read_counter();
    while (hpet_read_counter() < start + ticks)
    {
        asm volatile("pause");
    }
}
