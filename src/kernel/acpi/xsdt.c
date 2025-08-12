#include "xsdt.h"

#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "xsdt.h"

#include <assert.h>
#include <boot/boot_info.h>
#include <string.h>

static uint64_t tableAmount = 0;
static sdt_t** cachedTables = NULL;

static bool xsdt_is_sdt_valid(sdt_t* table)
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

static bool xsdt_is_valid(xsdt_t* xsdt)
{
    if (!xsdt_is_sdt_valid(&xsdt->header))
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

uint64_t xsdt_load_tables(xsdt_t* xsdt)
{
    assert(tableAmount == 0);
    assert(cachedTables == NULL);

    if (!xsdt_is_valid(xsdt))
    {
        LOG_ERR("invalid XSDT\n");
        return ERR;
    }

    tableAmount = (xsdt->header.length - sizeof(sdt_t)) / sizeof(sdt_t*);

    cachedTables = heap_alloc(tableAmount * sizeof(sdt_t*), HEAP_NONE);
    if (cachedTables == NULL)
    {
        LOG_ERR("failed to allocate memory for ACPI tables\n");
        return ERR;
    }

    for (uint64_t i = 0; i < tableAmount; i++)
    {
        sdt_t* table = xsdt->tables[i];

        if (!xsdt_is_sdt_valid(table))
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
            LOG_ERR("failed to allocate memory for ACPI table\n");
            return ERR;
        }

        memcpy(cachedTable, table, table->length);
        cachedTables[i] = cachedTable;

        char signature[5] = {0};
        memcpy(signature, table->signature, 4);

        char oemId[7] = {0};
        memcpy(oemId, table->oemId, 6);

        LOG_INFO("%s 0x%016lx 0x%06x v%02X %s\n", signature, table, table->length, table->revision, oemId);
    }

    return tableAmount;
}

sdt_t* xsdt_lookup(const char* signature, uint64_t n)
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
