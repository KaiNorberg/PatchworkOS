#include "rsdt.h"

#include "debug.h"
#include "splash.h"
#include "vmm.h"

#include <string.h>

static uint64_t tableAmount;
static xsdt_t* xsdt;

static bool rsdt_valid_checksum(void* table, uint64_t length)
{
    int8_t sum = 0;
    for (uint64_t i = 0; i < length; i++)
    {
        sum += ((int8_t*)table)[i];
    }

    return sum == 0;
}

void rsdt_init(xsdp_t* xsdp)
{
    xsdp = VMM_LOWER_TO_HIGHER(xsdp);

    DEBUG_ASSERT(xsdp->revision == ACPI_REVISION_2_0, "revision");
    DEBUG_ASSERT(rsdt_valid_checksum(xsdp, xsdp->length), "checksum");

    xsdt = VMM_LOWER_TO_HIGHER((void*)xsdp->xsdtAddress);
    tableAmount = (xsdt->header.length - sizeof(sdt_t)) / 8;

    for (uint64_t i = 0; i < tableAmount; i++)
    {
        sdt_t* table = VMM_LOWER_TO_HIGHER(xsdt->tables[i]);

        DEBUG_ASSERT(rsdt_valid_checksum(table, table->length), "table");
    }
}

sdt_t* rsdt_lookup(const char* signature)
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
