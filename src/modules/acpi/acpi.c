#include <kernel/fs/path.h>
#include <modules/acpi/acpi.h>

#include <modules/acpi/aml/aml.h>
#include <modules/acpi/devices.h>
#include <modules/acpi/tables.h>

#include <kernel/fs/mount.h>
#include <kernel/fs/namespace.h>
#include <kernel/init/boot_info.h>
#include <kernel/init/init.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/pmm.h>
#include <kernel/module/module.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>

#include <boot/boot_info.h>
#include <string.h>

static bool mountInitialzed = false;
static mount_t* acpi = NULL;

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
        acpi = sysfs_mount_new("acpi", NULL, MODE_PROPAGATE_PARENTS | MODE_PROPAGATE_CHILDREN | MODE_ALL_PERMS, NULL, NULL, NULL);
        if (acpi == NULL)
        {
            panic(NULL, "failed to initialize ACPI sysfs group");
        }

        mountInitialzed = true;
    }

    return REF(acpi->source);
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

uint64_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_DEVICE_ATTACH:
    {
        boot_info_t* bootInfo = boot_info_get();
        if (bootInfo == NULL || bootInfo->rsdp == NULL)
        {
            LOG_ERR("no RSDP provided by bootloader\n");
            return ERR;
        }

        if (acpi_tables_init(bootInfo->rsdp) == ERR)
        {
            LOG_ERR("failed to initialize ACPI tables\n");
            return ERR;
        }

        if (aml_init() == ERR)
        {
            LOG_ERR("failed to initialize AML subsystem\n");
            return ERR;
        }

        if (acpi_devices_init() == ERR)
        {
            LOG_ERR("failed to initialize ACPI devices\n");
            return ERR;
        }

        acpi_reclaim_memory(&bootInfo->memory.map);

        if (acpi_tables_expose() == ERR)
        {
            LOG_ERR("failed to expose ACPI tables via sysfs\n");
            return ERR;
        }

        if (aml_namespace_expose() == ERR)
        {
            LOG_ERR("failed to expose ACPI devices via sysfs\n");
            return ERR;
        }
    }
    break;
    default:
        break;
    }

    return 0;
}

MODULE_INFO("ACPI Module", "Kai Norberg",
    "ACPI subsystem providing ACPI table handling, AML parsing and device management", OS_VERSION, "MIT", "BOOT_RSDP");