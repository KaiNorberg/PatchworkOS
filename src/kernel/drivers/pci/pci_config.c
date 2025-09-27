#include "pci_config.h"

#include "acpi/tables.h"
#include "log/log.h"
#include "mem/vmm.h"

uint64_t pci_config_init(void)
{
    // Find the MCFG table
    mcfg_t* mcfg = (mcfg_t*)acpi_tables_lookup("MCFG", 0);
    if (mcfg == NULL)
    {
        LOG_ERR("MCFG table not found, hardware is incompatible with PCI-e\n");
        return ERR;
    }

    uint64_t entriesLength = mcfg->header.length - sizeof(mcfg_t);
    uint64_t entryCount = entriesLength / sizeof(pci_config_bar_t);
    if (entriesLength % sizeof(pci_config_bar_t) != 0)
    {
        LOG_ERR("MCFG table does not contain a whole number of entries\n");
        return ERR;
    }

    for (uint64_t i = 0; i < entryCount; i++)
    {
        pci_config_bar_t* entry = &mcfg->entries[i];

        uint64_t busCount = entry->endBus - entry->startBus + 1;
        uint64_t length = busCount * 256 * 4096;
        uint64_t pageAmount = BYTES_TO_PAGES(length);

        if (vmm_kernel_map((void*)entry->base, NULL, pageAmount, PML_WRITE) == NULL)
        {
            LOG_ERR("failed to map PCI-e configuration space at 0x%016lx\n", entry->base);
            return ERR;
        }

        LOG_INFO("mapped PCI-e config space 0x%016lx (segment=%u bus=%u-%u)\n", entry->base, entry->segmentGroup,
            entry->startBus, entry->endBus);
    }

    return 0;
}
