#include "hpet.h"

#include "acpi/tables.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/vmm.h"
#include "utils/utils.h"

#include <assert.h>

static hpet_t* hpet;
static uintptr_t address;
static uint64_t period; // Main counter tick period in femtoseconds (10^-15 s).

static bool isInitialized = false;

static uint64_t hpet_init(sdt_header_t* table)
{
    hpet = (hpet_t*)table;

    if (hpet->addressSpaceId != HPET_ADDRESS_SPACE_MEMORY)
    {
        LOG_ERR("HPET address space is not memory (id=%u) which is not supported\n", hpet->addressSpaceId);
        return ERR;
    }

    address = (uintptr_t)PML_LOWER_TO_HIGHER(hpet->address);
    if (vmm_map(NULL, (void*)address, (void*)hpet->address, PAGE_SIZE, PML_WRITE | PML_GLOBAL | PML_PRESENT, NULL,
            NULL) == NULL)
    {
        LOG_ERR("failed to map HPET memory at 0x%016lx\n", hpet->address);
        return ERR;
    }

    isInitialized = true;

    uint64_t capabilities = hpet_read(HPET_REG_GENERAL_CAPABILITIES_ID);
    period = capabilities >> HPET_CAP_COUNTER_CLK_PERIOD_SHIFT;

    if (period == 0 || period >= 0x05F5E100)
    {
        LOG_ERR("HPET reported an invalid counter period %llu fs\n", period);
        isInitialized = false;
        return ERR;
    }

    LOG_INFO("started HPET timer phys=0x%016lx virt=0x%016lx period=%lluns timers=%u %s-bit\n", hpet->address, address,
        period / (HPET_FEMTOSECONDS_PER_SECOND / CLOCKS_PER_SEC), hpet->comparatorCount + 1,
        hpet->counterIs64Bit ? "64" : "32");

    hpet_reset_counter();
    return 0;
}

ACPI_SDT_HANDLER_REGISTER("HPET", hpet_init);

clock_t hpet_nanoseconds_per_tick(void)
{
    if (!isInitialized)
    {
        return 0;
    }
    return period / (HPET_FEMTOSECONDS_PER_SECOND / CLOCKS_PER_SEC);
}

uint64_t hpet_read_counter(void)
{
    if (!isInitialized)
    {
        return 0;
    }
    return hpet_read(HPET_REG_MAIN_COUNTER_VALUE);
}

void hpet_reset_counter(void)
{
    if (!isInitialized)
    {
        return;
    }
    hpet_write(HPET_REG_GENERAL_CONFIG, 0);
    hpet_write(HPET_REG_MAIN_COUNTER_VALUE, 0);
    hpet_write(HPET_REG_GENERAL_CONFIG, HPET_CONF_ENABLE_CNF_BIT);
}

void hpet_write(uint64_t reg, uint64_t value)
{
    if (!isInitialized)
    {
        panic(NULL, "HPET not initialized");
    }
    WRITE_64(address + reg, value);
}

uint64_t hpet_read(uint64_t reg)
{
    if (!isInitialized)
    {
        panic(NULL, "HPET not initialized");
    }
    return READ_64(address + reg);
}

void hpet_wait(clock_t nanoseconds)
{
    if (!isInitialized)
    {
        panic(NULL, "HPET not initialized");
    }

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
