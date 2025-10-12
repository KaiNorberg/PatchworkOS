#include "acpi.h"

#include "aml/aml.h"
#include "devices.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/pmm.h"
#include "tables.h"

#include <boot/boot_info.h>
#include <string.h>

static sysfs_group_t acpiGroup;

static bool acpi_is_rsdp_valid(rsdp_t* rsdp)
{
    if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0)
    {
        LOG_ERR("invalid RSDP signature\n");
        return false;
    }

    if (!acpi_is_checksum_valid(rsdp, 20))
    {
        LOG_ERR("invalid RSDP checksum\n");
        return false;
    }

    if (rsdp->revision != RSDP_CURRENT_REVISION)
    {
        LOG_ERR("unsupported ACPI revision %u\n", rsdp->revision);
    }

    if (!acpi_is_checksum_valid(rsdp, rsdp->length))
    {
        LOG_ERR("invalid extended RSDP checksum\n");
        return false;
    }

    return true;
}

static void acpi_reclaim_memory(const boot_memory_map_t* map)
{
    for (uint64_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);

        if (desc->Type == EfiACPIReclaimMemory)
        {
            pmm_free_pages(PML_LOWER_TO_HIGHER(desc->PhysicalStart), desc->NumberOfPages);
            LOG_INFO("reclaim memory [0x%016lx-0x%016lx]\n", desc->PhysicalStart,
                ((uintptr_t)desc->PhysicalStart) + desc->NumberOfPages * PAGE_SIZE);
        }
    }
}

void acpi_init(rsdp_t* rsdp, const boot_memory_map_t* map)
{
    LOG_INFO("initializing acpi\n");

    if (sysfs_group_init(&acpiGroup, PATHNAME("/acpi")) == ERR)
    {
        panic(NULL, "failed to create '/acpi' sysfs group");
    }

    if (!acpi_is_rsdp_valid(rsdp))
    {
        panic(NULL, "invalid RSDP structure\n");
    }

    xsdt_t* xsdt = PML_LOWER_TO_HIGHER(rsdp->xsdtAddress);

    if (acpi_tables_init(xsdt) == ERR)
    {
        panic(NULL, "Failed to initialize ACPI tables");
    }

    if (aml_init() == ERR)
    {
        panic(NULL, "failed to initalize AML");
    }

    if (acpi_devices_init() == ERR)
    {
        panic(NULL, "failed to initialize ACPI devices");
    }

    acpi_reclaim_memory(map);
}

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
    return &acpiGroup.root;
}
