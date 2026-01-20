#include <kernel/fs/path.h>
#include <modules/acpi/acpi.h>

#include <modules/acpi/aml/aml.h>
#include <modules/acpi/devices.h>
#include <modules/acpi/tables.h>

#include <kernel/fs/mount.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/sysfs.h>
#include <kernel/init/boot_info.h>
#include <kernel/init/init.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/pmm.h>
#include <kernel/module/module.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>

#ifdef _TESTING_
#include <kernel/utils/test.h>
#endif

#include <boot/boot_info.h>
#include <string.h>

static bool dirInitialized = false;
static dentry_t* acpi = NULL;

bool acpi_is_checksum_valid(void* table, uint64_t length)
{
    uint8_t sum = 0;
    for (uint64_t i = 0; i < length; i++)
    {
        sum += ((uint8_t*)table)[i];
    }

    return sum == 0;
}

dentry_t* acpi_get_dir(void)
{
    if (!dirInitialized)
    {
        namespace_t* ns = process_get_ns(process_get_kernel());
        if (ns == NULL)
        {
            panic(NULL, "failed to get kernel process namespace for ACPI sysfs group");
        }
        UNREF_DEFER(ns);

        acpi = sysfs_dir_new(NULL, "acpi", NULL, NULL);
        if (acpi == NULL)
        {
            panic(NULL, "failed to initialize ACPI sysfs group");
        }

        dirInitialized = true;
    }

    return REF(acpi);
}

void acpi_reclaim_memory(const boot_memory_map_t* map)
{
    for (uint64_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);

        if (desc->Type == EfiACPIReclaimMemory)
        {
            pmm_free_region(PHYS_TO_PFN(desc->PhysicalStart), desc->NumberOfPages);
            LOG_INFO("reclaim memory [%p-%p]\n", desc->PhysicalStart,
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

#ifdef _TESTING_
#ifndef _ACPI_NOTEST_
        TEST_ALL();
#endif
#endif

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