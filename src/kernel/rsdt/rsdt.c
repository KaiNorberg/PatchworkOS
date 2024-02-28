#include "rsdt/rsdt.h"

#include "tty/tty.h"
#include "vmm/vmm.h"

#include <libc/string.h>

static uint64_t tableAmount;
static Xsdt* xsdt;

static inline uint8_t rsdt_validate_checksum(void* table, uint64_t length)
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

    xsdp = vmm_physical_to_virtual(xsdp);

    tty_assert(xsdp->revision == ACPI_REVISION_2_0, "Incompatible ACPI revision");
    tty_assert(rsdt_validate_checksum(xsdp, xsdp->length), "Invalid XSDP checksum");

    xsdt = vmm_physical_to_virtual((void*)xsdp->xsdtAddress);
    tableAmount = (xsdt->header.length - sizeof(SdtHeader)) / 8;

    for (uint64_t i = 0; i < tableAmount; i++)
    {
        SdtHeader* table = vmm_physical_to_virtual(xsdt->tables[i]);

        if (!rsdt_validate_checksum(table, table->length))
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
        SdtHeader* table = vmm_physical_to_virtual(xsdt->tables[i]);

        if (memcmp(table->signature, signature, 4) == 0)
        {
            return table;
        }
    }

    return 0;
}