#include "tables.h"

#include "acpi.h"
#include "fs/file.h"
#include "fs/vfs.h"
#include "log/log.h"
#include "mem/heap.h"

#include <errno.h>
#include <boot/boot_info.h>
#include <stdio.h>
#include <string.h>

static uint64_t ssdtAmount = 0;
static uint64_t tableAmount = 0;
static struct
{
    sdt_header_t* table;
    sysfs_file_t file;
} cachedTables[ACPI_MAX_TABLES] = {0};

static sysfs_dir_t apicTablesDir;

// Defined in the linker script
extern const acpi_sdt_handler_t _acpiSdtHandlersStart[];
extern const acpi_sdt_handler_t _acpiSdtHandlersEnd[];

static uint64_t acpi_table_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    if (file == NULL || buffer == NULL || offset == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    sdt_header_t* table = file->inode->private;
    if (table == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    return BUFFER_READ(buffer, count, offset, table, table->length);
}

static file_ops_t tableFileOps = {
    .read = acpi_table_read,
};

static bool acpi_is_table_valid(sdt_header_t* table)
{
    if (table->length < sizeof(sdt_header_t))
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

static uint64_t acpi_tables_push(sdt_header_t* table)
{
    if (!acpi_is_table_valid(table))
    {
        LOG_ERR("invalid table %.4s\n", table->signature);
        return ERR;
    }

    if (tableAmount >= ACPI_MAX_TABLES)
    {
        LOG_ERR("too many tables\n");
        return ERR;
    }

    sdt_header_t* cachedTable = heap_alloc(table->length, HEAP_NONE);
    if (cachedTable == NULL)
    {
        LOG_ERR("failed to allocate memory for ACPI table\n");
        return ERR;
    }

    memcpy(cachedTable, table, table->length);

    char name[MAX_PATH];
    if (memcmp(cachedTable->signature, "SSDT", 4) == 0)
    {
        snprintf(name, MAX_PATH, "%.4s%llu", cachedTable->signature, ssdtAmount++);
    }
    else
    {
        memcpy(name, cachedTable->signature, 4);
        name[4] = '\0';
    }

    if (sysfs_file_init(&cachedTables[tableAmount].file, &apicTablesDir, name, NULL, &tableFileOps, cachedTable) == ERR)
    {
        LOG_ERR("failed to create sysfs file for ACPI table %.4s\n", cachedTable->signature);
        heap_free(cachedTable);
        return ERR;
    }
    cachedTables[tableAmount++].table = cachedTable;

    return 0;
}

static uint64_t acpi_tables_load_from_xsdt(xsdt_t* xsdt)
{
    if (!acpi_is_xsdt_valid(xsdt))
    {
        LOG_ERR("invalid XSDT\n");
        return ERR;
    }

    uint64_t amountOfTablesInXsdt = (xsdt->header.length - sizeof(sdt_header_t)) / sizeof(sdt_header_t*);
    for (uint64_t i = 0; i < amountOfTablesInXsdt; i++)
    {
        sdt_header_t* table = xsdt->tables[i];
        if (acpi_tables_push(table) == ERR)
        {
            LOG_ERR("failed to cache table %.4s\n", table->signature);
            return ERR;
        }
    }

    return 0;
}

static uint64_t acpi_tables_load_from_fadt(void)
{
    fadt_t* facp = FADT_GET();
    if (facp == NULL)
    {
        LOG_ERR("failed to find FACP table\n");
        return ERR;
    }

    if (acpi_tables_push((void*)facp->xDsdt) == ERR)
    {
        LOG_ERR("failed to cache DSDT table\n");
        return ERR;
    }

    return 0;
}

static uint64_t acpi_tables_init_handlers(sdt_header_t* table)
{
    for (const acpi_sdt_handler_t* handler = _acpiSdtHandlersStart; handler < _acpiSdtHandlersEnd; handler++)
    {
        if (memcmp(table->signature, handler->signature, SDT_SIGNATURE_LENGTH) == 0)
        {
            if (handler->init(table) == ERR)
            {
                LOG_ERR("failed to initialize ACPI table %.4s\n", table->signature);
                return ERR;
            }
        }
    }

    return 0;
}

uint64_t acpi_tables_init(xsdt_t* xsdt)
{
    if (sysfs_dir_init(&apicTablesDir, acpi_get_sysfs_root(), "tables", NULL, NULL) == ERR)
    {
        LOG_ERR("failed to create '/acpi/tables' sysfs directory\n");
        return ERR;
    }

    if (acpi_tables_load_from_xsdt(xsdt) == ERR)
    {
        LOG_ERR("failed to load ACPI tables from XSDT\n");
        return ERR;
    }

    if (acpi_tables_load_from_fadt() == ERR)
    {
        LOG_ERR("failed to load ACPI tables from FADT\n");
        return ERR;
    }

    for (uint64_t i = 0; i < tableAmount; i++)
    {
        sdt_header_t* table = cachedTables[i].table;
        LOG_INFO("%.4s 0x%016lx 0x%06x v%02X %.6s\n", table->signature, table, table->length, table->revision,
            table->oemId);
        if (acpi_tables_init_handlers(table) == ERR)
        {
            return ERR;
        }
    }

    return tableAmount;
}

sdt_header_t* acpi_tables_lookup(const char* signature, uint64_t n)
{
    if (strlen(signature) != 4)
    {
        LOG_ERR("invalid signature length\n");
        return NULL;
    }

    for (uint64_t i = 0; i < tableAmount; i++)
    {
        if (memcmp(cachedTables[i].table->signature, signature, 4) == 0)
        {
            if (n-- == 0)
            {
                return cachedTables[i].table;
            }
        }
    }

    return NULL;
}
