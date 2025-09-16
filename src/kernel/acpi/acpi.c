#include "acpi.h"

#include "aml/aml.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "tables.h"

#include <assert.h>
#include <boot/boot_info.h>
#include <string.h>

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
        LOG_ERR("Failed to retrieve DSDT\n");
        return ERR;
    }

    LOG_INFO("DSDT found containing %llu bytes of AML code\n", dsdt->header.length - sizeof(dsdt_t));

    if (aml_init() == ERR)
    {
        LOG_ERR("failed to initialize AML\n");
        return ERR;
    }

    if (aml_parse(dsdt->definitionBlock, dsdt->header.length - sizeof(dsdt_t)) == ERR)
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

        LOG_INFO("SSDT %llu found containing %llu bytes of AML code\n", index, ssdt->header.length - sizeof(ssdt_t));

        if (aml_parse(ssdt->definitionBlock, ssdt->header.length - sizeof(ssdt_t)) == ERR)
        {
            LOG_ERR("failed to parse SSDT %llu\n", index);
            return ERR;
        }

        index++;
    }

    // For debugging, remove later
    LOG_INFO("==ACPI Namespace Tree==\n");
    aml_print_tree(aml_root_get(), 0, true);

    return 0;
}

void acpi_init(rsdp_t* rsdp, boot_memory_map_t* map)
{
    LOG_INFO("initializing acpi\n");

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
        panic(NULL, "Failed to load ACPI namespaces");
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
