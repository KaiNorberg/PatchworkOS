#include "acpi.h"

#include "gnu-efi/inc/efidef.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "xsdt.h"

#include <assert.h>
#include <boot/boot_info.h>
#include <string.h>

static bool acpi_is_xsdp_valid(xsdp_t* xsdp)
{
    if (memcmp(xsdp->signature, "RSD PTR ", 8) != 0)
    {
        LOG_ERR("invalid XSDP signature\n");
        return false;
    }

    if (!acpi_is_checksum_valid(xsdp, 20))
    {
        LOG_ERR("invalid XSDP checksum\n");
        return false;
    }

    if (xsdp->revision >= ACPI_REVISION_2_0)
    {
        if (!acpi_is_checksum_valid(xsdp, xsdp->length))
        {
            LOG_ERR("invalid extended XSDP checksum\n");
            return false;
        }
    }
    else if (xsdp->revision < ACPI_REVISION_2_0)
    {
        LOG_ERR("unsupported ACPI revision %u\n", xsdp->revision);
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

void acpi_init(xsdp_t* xsdp, boot_memory_map_t* map)
{
    LOG_INFO("initializing acpi\n");

    if (!acpi_is_xsdp_valid(xsdp))
    {
        panic(NULL, "invalid XSDP structure\n");
    }

    xsdt_t* xsdt = PML_LOWER_TO_HIGHER(xsdp->xsdtAddress);

    xsdt_load_tables(xsdt);

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
