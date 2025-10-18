#include "acpi.h"

#include "aml/aml.h"
#include "devices.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/pmm.h"
#include "tables.h"

#include <boot/boot_info.h>
#include <string.h>

static bool groupInitialized = false;
static sysfs_group_t acpiGroup;

bool acpi_is_checksum_valid(void* table, uint64_t length)
{
    uint8_t sum = 0;
    for (uint64_t i = 0; i < length; i++)
    {
        sum += ((uint8_t*)table)[i];
    }

    return sum == 0;
}

sysfs_dir_t* acpi_get_sysfs_root(void)
{
    if (!groupInitialized)
    {
        if (sysfs_group_init(&acpiGroup, PATHNAME("/acpi")) == ERR)
        {
            panic(NULL, "failed to initialize ACPI sysfs group");
        }

        groupInitialized = true;
    }

    return &acpiGroup.root;
}

void acpi_reclaim_memory(const boot_memory_map_t* map)
{
    for (uint64_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);

        if (desc->Type == EfiACPIReclaimMemory)
        {
            pmm_free_pages((void*)PML_LOWER_TO_HIGHER(desc->PhysicalStart), desc->NumberOfPages);
            LOG_INFO("reclaim memory [0x%016lx-0x%016lx]\n", desc->PhysicalStart,
                ((uintptr_t)desc->PhysicalStart) + desc->NumberOfPages * PAGE_SIZE);
        }
    }
}
