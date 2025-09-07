#include "acpi.h"

#include "aml.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "tables.h"

#include <assert.h>
#include <boot/boot_info.h>
#include <string.h>

static sysfs_group_t acpiGroup = {0};

static sysfs_dir_t sbDir = {0};
static sysfs_dir_t siDir = {0};
static sysfs_dir_t gpeDir = {0};
static sysfs_dir_t prDir = {0};
static sysfs_dir_t tzDir = {0};
static sysfs_dir_t osiDir = {0};
static sysfs_dir_t osDir = {0};
static sysfs_dir_t revDir = {0};

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

static uint64_t acpi_group_init(void)
{
    if (sysfs_group_init(&acpiGroup, PATHNAME("/acpi")) == ERR)
    {
        LOG_ERR("Failed to create ACPI sysfs group (%s)\n", strerror(errno));
        return ERR;
    }

    if (sysfs_dir_init(&sbDir, &acpiGroup.root, "_SB_", NULL, NULL) == ERR)
    {
        LOG_ERR("Failed to create ACPI sysfs directory (%s)\n", strerror(errno));
        return ERR;
    }

    if (sysfs_dir_init(&siDir, &acpiGroup.root, "_SI_", NULL, NULL) == ERR)
    {
        LOG_ERR("Failed to create ACPI sysfs directory (%s)\n", strerror(errno));
        return ERR;
    }

    if (sysfs_dir_init(&gpeDir, &acpiGroup.root, "_GPE", NULL, NULL) == ERR)
    {
        LOG_ERR("Failed to create ACPI sysfs directory (%s)\n", strerror(errno));
        return ERR;
    }

    if (sysfs_dir_init(&prDir, &acpiGroup.root, "_PR_", NULL, NULL) == ERR)
    {
        LOG_ERR("Failed to create ACPI sysfs directory (%s)\n", strerror(errno));
        return ERR;
    }

    if (sysfs_dir_init(&tzDir, &acpiGroup.root, "_TZ_", NULL, NULL) == ERR)
    {
        LOG_ERR("Failed to create ACPI sysfs directory (%s)\n", strerror(errno));
        return ERR;
    }

    if (sysfs_dir_init(&osiDir, &acpiGroup.root, "_OSI", NULL, NULL) == ERR)
    {
        LOG_ERR("Failed to create ACPI sysfs directory (%s)\n", strerror(errno));
        return ERR;
    }

    if (sysfs_dir_init(&osDir, &acpiGroup.root, "_OS_", NULL, NULL) == ERR)
    {
        LOG_ERR("Failed to create ACPI sysfs directory (%s)\n", strerror(errno));
        return ERR;
    }

    if (sysfs_dir_init(&revDir, &acpiGroup.root, "_REV", NULL, NULL) == ERR)
    {
        LOG_ERR("Failed to create ACPI sysfs directory (%s)\n", strerror(errno));
        return ERR;
    }

    return 0;
}

static uint64_t acpi_namespaces_load_from_aml(void)
{
    dsdt_t* dsdt = DSDT_GET();
    if (dsdt == NULL)
    {
        LOG_ERR("Failed to retrieve DSDT\n");
        return ERR;
    }

    LOG_INFO("DSDT found containing %llu bytes of AML code\n", dsdt->header.length - sizeof(dsdt_t));

    if (aml_parse(dsdt->data, dsdt->header.length - sizeof(dsdt_t)) == ERR)
    {
        LOG_ERR("Failed to parse DSDT\n");
        return ERR;
    }

    return 0;
}

void acpi_init(xsdp_t* xsdp, boot_memory_map_t* map)
{
    LOG_INFO("initializing acpi\n");

    if (!acpi_is_xsdp_valid(xsdp))
    {
        panic(NULL, "invalid XSDP structure\n");
    }

    xsdt_t* xsdt = PML_LOWER_TO_HIGHER(xsdp->xsdtAddress);

    if (acpi_tables_init(xsdt) == ERR)
    {
        panic(NULL, "Failed to initialize ACPI tables");
    }

    if (acpi_group_init() == ERR)
    {
        panic(NULL, "Failed to initialize ACPI sysfs group");
    }

    if (acpi_namespaces_load_from_aml() == ERR)
    {
        // For now we expect this to fail as its not fully implemented yet.
        // panic(NULL, "Failed to load ACPI namespaces");

        LOG_ERR("Failed to load ACPI namespaces (this is expected as it is not fully implemented yet!)\n");
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
