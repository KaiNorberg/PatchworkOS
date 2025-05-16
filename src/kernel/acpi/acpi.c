#include "acpi.h"

#include "utils/log.h"
#include "mem/vmm.h"

#include <stdio.h>
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

    ASSERT_PANIC(xsdp->revision == ACPI_REVISION_2_0);
    ASSERT_PANIC(acpi_valid_checksum(xsdp, xsdp->length));

    xsdt = VMM_LOWER_TO_HIGHER((void*)xsdp->xsdtAddress);
    tableAmount = (xsdt->header.length - sizeof(sdt_t)) / sizeof(void*);

    for (uint64_t i = 0; i < tableAmount; i++)
    {
        sdt_t* table = VMM_LOWER_TO_HIGHER(xsdt->tables[i]);

        char signature[5];
        memcpy(signature, table->signature, 4);
        signature[4] = '\0';

        char oemId[7];
        memcpy(oemId, table->oemId, 6);
        oemId[6] = '\0';

        printf("acpi: %s 0x%016lx 0x%06lx v%02X %-8s\n", signature, VMM_HIGHER_TO_LOWER(table), table->length,
            table->revision, oemId);

        ASSERT_PANIC_MSG(acpi_valid_checksum(table, table->length), "acpi: %s, invalid checksum\n", signature);
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
