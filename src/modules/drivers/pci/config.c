#include <kernel/drivers/pci/config.h>

#include <kernel/log/log.h>
#include <kernel/mem/vmm.h>
#include <kernel/acpi/tables.h>

static uint64_t entryCount;
static mcfg_t* mcfg;

static bool initialized = false;
static lock_t initLock = LOCK_CREATE();

static uint64_t pci_config_init(void)
{
    LOCK_SCOPE(&initLock);

    if (initialized)
    {
        return 0;
    }

    mcfg = (mcfg_t*)acpi_tables_lookup("MCFG", sizeof(mcfg_t), 0);
    if (mcfg == NULL)
    {
        LOG_ERR("no MCFG table found\n");
        return ERR;
    }

    uint64_t entriesLength = mcfg->header.length - sizeof(mcfg_t);
    if (entriesLength % sizeof(pci_config_bar_t) != 0)
    {
        LOG_ERR("MCFG table does not contain a whole number of entries\n");
        return ERR;
    }

    entryCount = entriesLength / sizeof(pci_config_bar_t);

    for (uint64_t i = 0; i < entryCount; i++)
    {
        pci_config_bar_t* entry = &mcfg->entries[i];

        uint64_t busCount = entry->endBus - entry->startBus + 1;
        uint64_t length = busCount * 256 * 4096;

        void* virtAddr = (void*)PML_LOWER_TO_HIGHER(entry->base);
        if (vmm_map(NULL, virtAddr, entry->base, length, PML_WRITE | PML_GLOBAL | PML_PRESENT, NULL, NULL) == NULL)
        {
            LOG_ERR("failed to map PCI-e configuration space at %p\n", entry->base);
            return ERR;
        }

        LOG_INFO("mapped PCI-e config space %p (segment=%u bus=%u-%u)\n", entry->base, entry->segmentGroup,
            entry->startBus, entry->endBus);
    }

    errno = EOK;
    initialized = true;
    return 0;
}

static pci_config_bar_t* pci_config_bar_get(pci_segment_group_t segmentGroup, pci_bus_t bus)
{
    if (pci_config_init() == ERR)
    {
        return NULL;
    }

    for (uint64_t i = 0; i < entryCount; ++i)
    {
        if (mcfg->entries[i].segmentGroup == segmentGroup && bus >= mcfg->entries[i].startBus &&
            bus <= mcfg->entries[i].endBus)
        {
            return &mcfg->entries[i];
        }
    }
    return NULL;
}

static volatile void* pci_config_get_address(pci_segment_group_t segmentGroup, pci_bus_t bus, pci_slot_t slot,
    uint8_t function, uint16_t offset)
{
    if (pci_config_init() == ERR)
    {
        return NULL;
    }

    pci_config_bar_t* region = pci_config_bar_get(segmentGroup, bus);
    if (region == NULL)
    {
        return NULL;
    }

    // This formula calculates the final address based on the OSDev Wiki page.
    uintptr_t address = (uintptr_t)PML_LOWER_TO_HIGHER(region->base);
    address += (uint64_t)(bus - region->startBus) << 20;
    address += (uint64_t)slot << 15;
    address += (uint64_t)function << 12;
    address += offset;

    return (void*)address;
}

uint8_t pci_config_read8(pci_segment_group_t segmentGroup, pci_bus_t bus, pci_slot_t slot, pci_function_t function,
    uint16_t offset)
{
    if (pci_config_init() == ERR)
    {
        return 0xFF;
    }

    volatile uint8_t* addr = (volatile uint8_t*)pci_config_get_address(segmentGroup, bus, slot, function, offset);
    if (addr == NULL)
    {
        return 0xFF;
    }
    return *addr;
}

uint16_t pci_config_read16(pci_segment_group_t segmentGroup, pci_bus_t bus, pci_slot_t slot, pci_function_t function,
    uint16_t offset)
{
    if (pci_config_init() == ERR)
    {
        return 0xFFFF;
    }

    volatile uint16_t* addr = (volatile uint16_t*)pci_config_get_address(segmentGroup, bus, slot, function, offset);
    if (addr == NULL)
    {
        return 0xFFFF;
    }
    return *addr;
}

uint32_t pci_config_read32(pci_segment_group_t segmentGroup, pci_bus_t bus, pci_slot_t slot, pci_function_t function,
    uint16_t offset)
{
    if (pci_config_init() == ERR)
    {
        return 0xFFFFFFFF;
    }

    volatile uint32_t* addr = (volatile uint32_t*)pci_config_get_address(segmentGroup, bus, slot, function, offset);
    if (addr == NULL)
    {
        return 0xFFFFFFFF;
    }
    return *addr;
}

void pci_config_write8(pci_segment_group_t segmentGroup, pci_bus_t bus, pci_slot_t slot, pci_function_t function,
    uint16_t offset, uint8_t value)
{
    volatile uint8_t* addr = (volatile uint8_t*)pci_config_get_address(segmentGroup, bus, slot, function, offset);
    if (addr != NULL)
    {
        *addr = value;
    }
}

void pci_config_write16(pci_segment_group_t segmentGroup, pci_bus_t bus, pci_slot_t slot, pci_function_t function,
    uint16_t offset, uint16_t value)
{
    if (pci_config_init() == ERR)
    {
        return;
    }

    volatile uint16_t* addr = (volatile uint16_t*)pci_config_get_address(segmentGroup, bus, slot, function, offset);
    if (addr != NULL)
    {
        *addr = value;
    }
}

void pci_config_write32(pci_segment_group_t segmentGroup, pci_bus_t bus, pci_slot_t slot, pci_function_t function,
    uint16_t offset, uint32_t value)
{
    if (pci_config_init() == ERR)
    {
        return;
    }

    volatile uint32_t* addr = (volatile uint32_t*)pci_config_get_address(segmentGroup, bus, slot, function, offset);
    if (addr != NULL)
    {
        *addr = value;
    }
}
