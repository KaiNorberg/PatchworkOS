#include "rsdt/rsdt.h"

#include "tty/tty.h"

static SdtHeader* xsdt; 

void rsdt_init(Xsdt* xsdp)
{
    tty_start_message("RSDT initializing");

    xsdt = (SdtHeader*)xsdp->xsdtAddress;

    tty_end_message(TTY_MESSAGE_OK);
}

SdtHeader* rsdt_lookup(const char* signature)
{
    uint64_t entryAmount = (xsdt->length - sizeof(SdtHeader)) / 8;

    for (uint64_t i = 0; i < entryAmount; i++)
    {
        SdtHeader* header = (SdtHeader*)*(uint64_t*)((uint64_t)xsdt + sizeof(SdtHeader) + i * 8);

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