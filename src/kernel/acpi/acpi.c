#include "acpi.h"

#include "tty/tty.h"

SDTHeader* xsdt; 

void acpi_init(XSDP* xsdp)
{
    tty_start_message("ACPI initializing");

    xsdt = (SDTHeader*)xsdp->xsdtAddress;

    tty_end_message(TTY_MESSAGE_OK);
}

SDTHeader* acpi_find(const char* signature)
{
    uint64_t entryAmount = (xsdt->length - sizeof(SDTHeader)) / 8;

    for (uint64_t i = 0; i < entryAmount; i++)
    {
        SDTHeader* header = (SDTHeader*)*(uint64_t*)((uint64_t)xsdt + sizeof(SDTHeader) + i * 8);

        for (int j = 0; j < 4; j++)
        {
            if (header->signature[j] != signature[j])
            {
                break;
            }
            else if (j == 3)
            {
                return header;
            }
        }
    }

    return 0;
}