#include "acpi.h"

#include "log.h"
#include "vmm.h"

#include <string.h>

static uint64_t tableAmount;
static xsdt_t* xsdt;

static bool acpi_valid_checksum(void* table, uint64_t length)
{
    int8_t sum = 0;
    for (uint64_t i = 0; i < length; i++)
    {
        sum += ((int8_t*)table)[i];
    }

    return sum == 0;
}

void acpi_init(xsdp_t* xsdp)
{
    xsdp = VMM_LOWER_TO_HIGHER(xsdp);

    LOG_ASSERT(xsdp->revision == ACPI_REVISION_2_0, "Invalid ACPI revision");
    LOG_ASSERT(acpi_valid_checksum(xsdp, xsdp->length), "Invalid XSDP checksum");

    xsdt = VMM_LOWER_TO_HIGHER((void*)xsdp->xsdtAddress);
    tableAmount = (xsdt->header.length - sizeof(sdt_t)) / sizeof(void*);

    log_print("Found ACPI tables:");
    for (uint64_t i = 0; i < tableAmount; i++)
    {
        sdt_t* table = VMM_LOWER_TO_HIGHER(xsdt->tables[i]);

        char signature[5];
        memcpy(signature, table->signature, 4);
        signature[4] = '\0';
        log_print("ACPI: %s %a", signature, VMM_HIGHER_TO_LOWER(table));

        LOG_ASSERT(acpi_valid_checksum(table, table->length), "ACPI: %s, invalid checksum", signature);
    }
}

sdt_t* acpi_lookup(const char* signature)
{
    for (uint64_t i = 0; i < tableAmount; i++)
    {
        sdt_t* table = VMM_LOWER_TO_HIGHER(xsdt->tables[i]);

        if (memcmp(table->signature, signature, 4) == 0)
        {
            return table;
        }
    }

    return NULL;
}
