#include "rsdt/rsdt.h"

#include <string.h>

#include "tty/tty.h"
#include "vmm/vmm.h"

static uint64_t tableAmount;
static Xsdt* xsdt;

static bool rsdt_valid_checksum(void* table, uint64_t length)
{
    int8_t sum = 0;
    for (uint64_t i = 0; i < length; i++)
    {
        sum += ((int8_t*)table)[i];
    }

    return sum == 0;
}

void rsdt_init(Xsdp* xsdp)
{
    tty_start_message("Parsing RSDT");

    xsdp = VMM_LOWER_TO_HIGHER(xsdp);

    tty_assert(xsdp->revision == ACPI_REVISION_2_0, "Incompatible ACPI revision");
    tty_assert(rsdt_valid_checksum(xsdp, xsdp->length), "Invalid XSDP checksum");

    xsdt = VMM_LOWER_TO_HIGHER((void*)xsdp->xsdtAddress);
    tableAmount = (xsdt->header.length - sizeof(SdtHeader)) / 8;

    for (uint64_t i = 0; i < tableAmount; i++)
    {
        SdtHeader* table = VMM_LOWER_TO_HIGHER(xsdt->tables[i]);

        if (!rsdt_valid_checksum(table, table->length))
        {
            tty_print("Invalid ");
            tty_printm((const char*)table->signature, 4);
            tty_print(" checksum");
            tty_end_message(TTY_MESSAGE_ER);
        }
    }

    tty_end_message(TTY_MESSAGE_OK);
}

SdtHeader* rsdt_lookup(const char* signature)
{   
    for (uint64_t i = 0; i < tableAmount; i++)
    {
        SdtHeader* table = VMM_LOWER_TO_HIGHER(xsdt->tables[i]);

        if (memcmp(table->signature, signature, 4) == 0)
        {
            return table;
        }
    }

    return NULL;
}