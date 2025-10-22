#include "acpi.h"

#include "aml/aml.h"
#include "devices.h"
#include "fs/mount.h"
#include "fs/superblock.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/pmm.h"
#include "tables.h"

#include <boot/boot_info.h>
#include <string.h>

static bool superInitialized = false;
static superblock_t* superblock = NULL;

bool acpi_is_checksum_valid(void* table, uint64_t length)
{
    uint8_t sum = 0;
    for (uint64_t i = 0; i < length; i++)
    {
        sum += ((uint8_t*)table)[i];
    }

    return sum == 0;
}

dentry_t* acpi_get_sysfs_root(void)
{
    if (!superInitialized)
    {
        superblock = sysfs_superblock_new(NULL, "acpi", NULL, NULL);
        if (superblock == NULL)
        {
            panic(NULL, "failed to initialize ACPI sysfs group");
        }

        superInitialized = true;
    }

    return REF(superblock->root);
}

void acpi_reclaim_memory(const boot_memory_map_t* map)
{
    for (uint64_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);

        if (desc->Type == EfiACPIReclaimMemory)
        {
            pmm_free_region((void*)PML_LOWER_TO_HIGHER(desc->PhysicalStart), desc->NumberOfPages);
            LOG_INFO("reclaim memory [0x%016lx-0x%016lx]\n", desc->PhysicalStart,
                ((uintptr_t)desc->PhysicalStart) + desc->NumberOfPages * PAGE_SIZE);
        }
    }
}
