#include "pci_config.h"

#include "acpi/tables.h"
#include "log/log.h"
#include "mem/vmm.h"

static uint64_t entryCount;
static mcfg_t* mcfg;

uint64_t pci_config_init(sdt_header_t* table)
{
    mcfg = (mcfg_t*)table;

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
        if (vmm_map(NULL, virtAddr, (void*)entry->base, length, PML_WRITE | PML_GLOBAL | PML_PRESENT, NULL, NULL) ==
            NULL)
        {
            LOG_ERR("failed to map PCI-e configuration space at 0x%016lx\n", entry->base);
            return ERR;
        }

        LOG_INFO("mapped PCI-e config space 0x%016lx (segment=%u bus=%u-%u)\n", entry->base, entry->segmentGroup,
            entry->startBus, entry->endBus);
    }

    errno = 0;
    return 0;
}

ACPI_SDT_HANDLER_REGISTER("MCFG", pci_config_init);

static pci_config_bar_t* pci_config_bar_get(pci_segment_group_t segmentGroup, pci_bus_t bus)
{
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
    volatile uint16_t* addr = (volatile uint16_t*)pci_config_get_address(segmentGroup, bus, slot, function, offset);
    if (addr != NULL)
    {
        *addr = value;
    }
}

void pci_config_write32(pci_segment_group_t segmentGroup, pci_bus_t bus, pci_slot_t slot, pci_function_t function,
    uint16_t offset, uint32_t value)
{
    volatile uint32_t* addr = (volatile uint32_t*)pci_config_get_address(segmentGroup, bus, slot, function, offset);
    if (addr != NULL)
    {
        *addr = value;
    }
}
