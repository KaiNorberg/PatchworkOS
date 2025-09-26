#include "acpi.h"

#include "aml/aml.h"
#include "aml/aml_patch_up.h"
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

static void acpi_reclaim_memory(boot_memory_map_t* map)
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

static uint64_t acpi_parse_all_aml(void)
{
    dsdt_t* dsdt = DSDT_GET();
    if (dsdt == NULL)
    {
        LOG_ERR("failed to retrieve DSDT\n");
        return ERR;
    }

    LOG_INFO("DSDT found containing %llu bytes of AML code\n", dsdt->header.length - sizeof(dsdt_t));

    if (aml_init() == ERR)
    {
        LOG_ERR("failed to initialize AML\n");
        return ERR;
    }

    const uint8_t* dsdtEnd = (const uint8_t*)dsdt + dsdt->header.length;
    if (aml_parse(dsdt->definitionBlock, dsdtEnd) == ERR)
    {
        LOG_ERR("failed to parse DSDT\n");
        return ERR;
    }

    uint64_t index = 0;
    ssdt_t* ssdt = NULL;
    while (true)
    {
        ssdt = SSDT_GET(index);
        if (ssdt == NULL)
        {
            break;
        }

        LOG_INFO("SSDT%llu found containing %llu bytes of AML code\n", index, ssdt->header.length - sizeof(ssdt_t));

        const uint8_t* ssdtEnd = (const uint8_t*)ssdt + ssdt->header.length;
        if (aml_parse(ssdt->definitionBlock, ssdtEnd) == ERR)
        {
            LOG_ERR("failed to parse SSDT%llu\n", index);
            return ERR;
        }

        index++;
    }

    LOG_INFO("parsed 1 DSDT and %llu SSDTs\n", index);

    LOG_INFO("resolving %llu unresolved nodes\n", aml_patch_up_unresolved_count());
    if (aml_patch_up_resolve_all() == ERR)
    {
        LOG_ERR("Failed to resolve unresolved nodes\n");
        return ERR;
    }

    if (aml_patch_up_unresolved_count() > 0)
    {
        LOG_ERR("There are still %llu unresolved nodes\n", aml_patch_up_unresolved_count());
        return ERR;
    }

    return 0;
}

void acpi_init(rsdp_t* rsdp, boot_memory_map_t* map)
{
    LOG_INFO("initializing acpi\n");

    if (sysfs_group_init(&acpiGroup, PATHNAME("/acpi")) == ERR)
    {
        panic(NULL, "failed to create '/acpi' sysfs group\n");
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

    if (acpi_parse_all_aml() == ERR)
    {
#ifdef NDEBUG
        LOG_WARN("failed to parse all AML code, since ACPI is still WIP we will continue booting but there may be "
                 "issues!\n");
#else
        panic(NULL, "failed to parse all AML code\n");
#endif
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
