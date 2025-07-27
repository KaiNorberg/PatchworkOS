#include "acpi.h"

#include "gnu-efi/inc/efidef.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "mem/pmm.h"

#include <assert.h>
#include <boot/boot_info.h>
#include <string.h>

static uint64_t tableAmount;
static sdt_t** cachedTables;
static xsdt_t* xsdt;

static bool acpi_is_checksum_valid(void* table, uint64_t length)
{
    uint8_t sum = 0;
    for (uint64_t i = 0; i < length; i++)
    {
        sum += ((uint8_t*)table)[i];
    }

    return sum == 0;
}

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

static bool acpi_is_sdt_valid(sdt_t* table)
{
    if (table->length < sizeof(sdt_t))
    {
        LOG_ERR("table too small (%u bytes)\n", table->length);
        return false;
    }

    if (!acpi_is_checksum_valid(table, table->length))
    {
        char sig[5] = {0};
        memcpy(sig, table->signature, 4);
        LOG_ERR("invalid checksum for table %s\n", sig);
        return false;
    }

    return true;
}

static bool acpi_is_xsdt_valid(xsdt_t* xsdt)
{
    if (!acpi_is_sdt_valid(&xsdt->header))
    {
        LOG_ERR("invalid checksum for xsdt\n");
        return false;
    }

    if (memcmp(xsdt->header.signature, "XSDT", 4) != 0)
    {
        LOG_ERR("invalid XSDT signature\n");
        return false;
    }

    return true;
}

static void acpi_load_tables(void)
{
    tableAmount = (xsdt->header.length - sizeof(sdt_t)) / sizeof(sdt_t*);

    cachedTables = heap_alloc(tableAmount * sizeof(sdt_t*), HEAP_NONE);
    if (cachedTables == NULL)
    {
        panic(NULL, "Failed to allocate memory for ACPI tables\n");
    }

    for (uint64_t i = 0; i < tableAmount; i++)
    {
        sdt_t* table = xsdt->tables[i];

        if (!acpi_is_sdt_valid(table))
        {
            char sig[5];
            memcpy(sig, table->signature, 4);
            sig[4] = '\0';
            LOG_WARN("skipping invalid table %s\n", sig);
            cachedTables[i] = NULL;
            continue;
        }

        sdt_t* cachedTable = heap_alloc(table->length, HEAP_NONE);
        if (cachedTable == NULL)
        {
            panic(NULL, "Failed to allocate memory for ACPI table\n");
        }

        memcpy(cachedTable, table, table->length);
        cachedTables[i] = cachedTable;

        char signature[5] = {0};
        memcpy(signature, table->signature, 4);

        char oemId[7] = {0};
        memcpy(oemId, table->oemId, 6);

        LOG_INFO("%s 0x%016lx 0x%06x v%02X %s\n", signature, table, table->length, table->revision, oemId);
    }
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
        panic(NULL, "Invalid XSDP structure\n");
    }

    xsdt = PML_LOWER_TO_HIGHER(xsdp->xsdtAddress);

    if (!acpi_is_xsdt_valid(xsdt))
    {
        panic(NULL, "Invalid XSDT\n");
    }

    acpi_load_tables();

    acpi_reclaim_memory(map);
}

sdt_t* acpi_lookup(const char* signature)
{
    if (strlen(signature) != 4)
    {
        LOG_ERR("invalid signature length\n");
        return NULL;
    }

    for (uint64_t i = 0; i < tableAmount; i++)
    {
        if (memcmp(cachedTables[i]->signature, signature, 4) == 0)
        {
            return cachedTables[i];
        }
    }

    return NULL;
}
