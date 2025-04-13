#include "acpi.h"

#include "log.h"
#include "vmm.h"

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
        printf("acpi: %s at %p", signature, VMM_HIGHER_TO_LOWER(table));

        ASSERT_PANIC_MSG(acpi_valid_checksum(table, table->length), "acpi: %s, invalid checksum", signature);
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
