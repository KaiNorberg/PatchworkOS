#include <kernel/mem/paging_types.h>
#include <kernel/acpi/tables.h>

#include <kernel/fs/file.h>
#include <kernel/fs/sysfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/acpi/acpi.h>

#include <boot/boot_info.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t ssdtAmount = 0;
static uint64_t tableAmount = 0;
static acpi_cached_table_t* cachedTables = NULL;

static dentry_t* tablesDir = NULL;

static uint64_t acpi_table_read(file_t* file, void* buffer, size_t count, size_t* offset)
{
    if (file == NULL || buffer == NULL || offset == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    sdt_header_t* table = file->vnode->data;
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
        LOG_ERR("invalid checksum for table %.*s\n", SDT_SIGNATURE_LENGTH, table->signature);
        return false;
    }

    return true;
}

static bool acpi_is_xsdt_valid(xsdt_t* xsdt)
{
    if (!acpi_is_table_valid(&xsdt->header))
    {
        return false;
    }

    if (memcmp(xsdt->header.signature, "XSDT", SDT_SIGNATURE_LENGTH) != 0)
    {
        LOG_ERR("invalid XSDT signature\n");
        return false;
    }

    return true;
}

static bool acpi_is_rsdp_valid(rsdp_t* rsdp)
{
    LOG_DEBUG("validating RSDP at [%p-%p]\n", rsdp, (void*)((uintptr_t)rsdp + rsdp->length));

    if (memcmp(rsdp->signature, "RSD PTR ", RSDP_SIGNATURE_LENGTH) != 0)
    {
        LOG_ERR("invalid RSDP signature\n");
        return false;
    }

    if (!acpi_is_checksum_valid(rsdp, RSDP_V1_LENGTH))
    {
        LOG_ERR("invalid RSDP checksum\n");
        return false;
    }

    if (rsdp->revision != RSDP_CURRENT_REVISION)
    {
        LOG_ERR("unsupported ACPI revision %u\n", rsdp->revision);
        return false;
    }

    if (!acpi_is_checksum_valid(rsdp, rsdp->length))
    {
        LOG_ERR("invalid extended RSDP checksum\n");
        return false;
    }

    return true;
}

static uint64_t acpi_tables_push(sdt_header_t* table)
{
    if (!acpi_is_table_valid(table))
    {
        LOG_ERR("invalid table %.*s\n", SDT_SIGNATURE_LENGTH, table->signature);
        return ERR;
    }

    sdt_header_t* cachedTable = malloc(table->length);
    if (cachedTable == NULL)
    {
        LOG_ERR("failed to allocate memory for ACPI table\n");
        return ERR;
    }
    memcpy(cachedTable, table, table->length);

    cachedTables = realloc(cachedTables, sizeof(acpi_cached_table_t) * (tableAmount + 1));
    if (cachedTables == NULL)
    {
        LOG_ERR("failed to allocate memory for ACPI table cache\n");
        free(cachedTable);
        return ERR;
    }
    cachedTables[tableAmount++].table = cachedTable;

    LOG_INFO("%.*s %p 0x%06x v%02X %.*s\n", SDT_SIGNATURE_LENGTH, cachedTable->signature, cachedTable,
        cachedTable->length, cachedTable->revision, SDT_OEM_ID_LENGTH, cachedTable->oemId);
    return 0;
}

static uint64_t acpi_tables_load_from_xsdt(xsdt_t* xsdt)
{
    if (!acpi_is_xsdt_valid(xsdt))
    {
        return ERR;
    }

    uint64_t amountOfTablesInXsdt = (xsdt->header.length - sizeof(sdt_header_t)) / sizeof(sdt_header_t*);
    for (uint64_t i = 0; i < amountOfTablesInXsdt; i++)
    {
        sdt_header_t* table = (sdt_header_t*)PML_ENSURE_HIGHER_HALF(xsdt->tables[i]);
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
    fadt_t* fadt = (fadt_t*)acpi_tables_lookup(FADT_SIGNATURE, sizeof(fadt_t), 0);
    if (fadt == NULL)
    {
        LOG_ERR("failed to find FACP table\n");
        return ERR;
    }

    if (fadt->dsdt == 0 && fadt->xDsdt == 0)
    {
        LOG_ERR("FADT has no DSDT pointer\n");
        return ERR;
    }

    if (fadt->dsdt == 0)
    {
        if (acpi_tables_push((void*)PML_ENSURE_HIGHER_HALF(fadt->xDsdt)) == ERR)
        {
            LOG_ERR("failed to cache DSDT table from fadt_t::xDsdt\n");
            return ERR;
        }
    }

    if (acpi_tables_push((void*)PML_ENSURE_HIGHER_HALF(fadt->dsdt)) == ERR)
    {
        LOG_ERR("failed to cache DSDT table from fadt_t::dsdt\n");
        return ERR;
    }

    return 0;
}

uint64_t acpi_tables_init(rsdp_t* rsdp)
{
    if (!acpi_is_rsdp_valid(rsdp))
    {
        LOG_ERR("invalid RSDP provided to ACPI tables init\n");
        return ERR;
    }

    xsdt_t* xsdt = (xsdt_t*)PML_ENSURE_HIGHER_HALF(rsdp->xsdtAddress);
    LOG_INFO("located XSDT at %p\n", rsdp->xsdtAddress);

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

    return 0;
}

uint64_t acpi_tables_expose(void)
{
    dentry_t* acpiRoot = acpi_get_dir();
    assert(acpiRoot != NULL);
    UNREF_DEFER(acpiRoot);

    tablesDir = sysfs_dir_new(acpiRoot, "tables", NULL, NULL);
    if (tablesDir == NULL)
    {
        LOG_ERR("failed to create ACPI tables sysfs directory");
        return ERR;
    }

    for (uint64_t i = 0; i < tableAmount; i++)
    {
        sdt_header_t* table = cachedTables[i].table;

        char name[SDT_SIGNATURE_LENGTH + 2];
        if (memcmp(table->signature, "SSDT", SDT_SIGNATURE_LENGTH) == 0)
        {
            snprintf(name, MAX_PATH, "%.4s%llu", table->signature, ssdtAmount++);
        }
        else
        {
            memcpy(name, table->signature, SDT_SIGNATURE_LENGTH);
            name[SDT_SIGNATURE_LENGTH] = '\0';
        }

        cachedTables[i].file = sysfs_file_new(tablesDir, name, NULL, &tableFileOps, table);
        if (cachedTables[i].file == NULL)
        {
            LOG_ERR("failed to create ACPI table sysfs file for %.*s", SDT_SIGNATURE_LENGTH, table->signature);
            return ERR;
        }
    }

    return 0;
}

sdt_header_t* acpi_tables_lookup(const char* signature, uint64_t minSize, uint64_t n)
{
    if (signature == NULL || strlen(signature) != SDT_SIGNATURE_LENGTH)
    {
        errno = EINVAL;
        return NULL;
    }

    uint64_t depth = 0;
    for (uint64_t i = 0; i < tableAmount; i++)
    {
        if (memcmp(cachedTables[i].table->signature, signature, SDT_SIGNATURE_LENGTH) == 0)
        {
            if (depth++ == n)
            {
                if (cachedTables[i].table->length < minSize)
                {
                    errno = EILSEQ;
                    return NULL;
                }

                return cachedTables[i].table;
            }
        }
    }

    if (depth != 0)
    {
        errno = ERANGE;
    }
    else
    {
        errno = ENOENT;
    }
    return NULL;
}
