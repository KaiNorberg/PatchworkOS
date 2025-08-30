#include "tables.h"

#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "acpi.h"

#include <assert.h>
#include <boot/boot_info.h>
#include <string.h>

static uint64_t tableAmount = 0;
static acpi_header_t** cachedTables = NULL;

static bool acpi_is_table_valid(acpi_header_t* table)
{
    if (table->length < sizeof(acpi_header_t))
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
    if (!acpi_is_table_valid(&xsdt->header))
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

static uint64_t acpi_tables_load_from_xsdt(xsdt_t* xsdt)
{
    if (!acpi_is_xsdt_valid(xsdt))
    {
        LOG_ERR("invalid XSDT\n");
        return ERR;
    }

    tableAmount = (xsdt->header.length - sizeof(acpi_header_t)) / sizeof(acpi_header_t*);

    cachedTables = heap_alloc(tableAmount * sizeof(acpi_header_t*), HEAP_NONE);
    if (cachedTables == NULL)
    {
        LOG_ERR("failed to allocate memory for ACPI tables\n");
        return ERR;
    }

    for (uint64_t i = 0; i < tableAmount; i++)
    {
        acpi_header_t* table = xsdt->tables[i];

        if (!acpi_is_table_valid(table))
        {
            char sig[5];
            memcpy(sig, table->signature, 4);
            sig[4] = '\0';
            LOG_WARN("skipping invalid table %s\n", sig);
            cachedTables[i] = NULL;
            continue;
        }

        acpi_header_t* cachedTable = heap_alloc(table->length, HEAP_NONE);
        if (cachedTable == NULL)
        {
            LOG_ERR("failed to allocate memory for ACPI table\n");
            return ERR;
        }

        memcpy(cachedTable, table, table->length);
        cachedTables[i] = cachedTable;
    }

    return tableAmount;
}

uint64_t acpi_tables_init(xsdt_t* xsdt)
{
    uint64_t tableAmount = acpi_tables_load_from_xsdt(xsdt);
    if (tableAmount == ERR)
    {
        LOG_ERR("failed to load ACPI tables from XSDT\n");
        return ERR;
    }

    return tableAmount;
}

acpi_header_t* acpi_tables_lookup(const char* signature, uint64_t n)
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
            if (n-- == 0)
            {
                return cachedTables[i];
            }
        }
    }

    return NULL;
}
