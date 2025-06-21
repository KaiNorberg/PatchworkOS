#include "acpi.h"

#include "log/log.h"
#include "mem/vmm.h"

#include <assert.h>
#include <string.h>

static uint64_t tableAmount;
static xsdt_t* xsdt;

static bool acpi_is_checksum_valid(void* table, uint64_t length)
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
    xsdp = PML_LOWER_TO_HIGHER(xsdp);

    assert(xsdp->revision == ACPI_REVISION_2_0);
    assert(acpi_is_checksum_valid(xsdp, xsdp->length));

    xsdt = PML_LOWER_TO_HIGHER((void*)xsdp->xsdtAddress);
    tableAmount = (xsdt->header.length - sizeof(sdt_t)) / sizeof(void*);

    for (uint64_t i = 0; i < tableAmount; i++)
    {
        sdt_t* table = PML_LOWER_TO_HIGHER(xsdt->tables[i]);

        char signature[5];
        memcpy(signature, table->signature, 4);
        signature[4] = '\0';

        char oemId[7];
        memcpy(oemId, table->oemId, 6);
        oemId[6] = '\0';

        log_print(LOG_INFO, "acpi: %s 0x%016lx 0x%06lx v%02X %-8s\n", signature, PML_HIGHER_TO_LOWER(table),
            table->length, table->revision, oemId);

        if (!acpi_is_checksum_valid(table, table->length))
        {
            log_panic(NULL, "acpi: %s, invalid checksum", signature);
        }
    }
}

sdt_t* acpi_lookup(const char* signature)
{
    for (uint64_t i = 0; i < tableAmount; i++)
    {
        sdt_t* table = PML_LOWER_TO_HIGHER(xsdt->tables[i]);

        if (memcmp(table->signature, signature, 4) == 0)
        {
            return table;
        }
    }

    return NULL;
}
