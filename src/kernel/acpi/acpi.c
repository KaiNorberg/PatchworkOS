#include <kernel/acpi/acpi.h>

#include <kernel/acpi/aml/aml.h>
#include <kernel/acpi/devices.h>
#include <kernel/acpi/tables.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/namespace.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/pmm.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>

#include <boot/boot_info.h>
#include <string.h>

static bool mountInitialzed = false;
static mount_t* mount = NULL;

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
    if (!mountInitialzed)
    {
        mount = sysfs_mount_new(NULL, "acpi", NULL, MOUNT_PROPAGATE_CHILDREN | MOUNT_PROPAGATE_PARENT, NULL);
        if (mount == NULL)
        {
            panic(NULL, "failed to initialize ACPI sysfs group");
        }

        mountInitialzed = true;
    }

    return REF(mount->root);
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
