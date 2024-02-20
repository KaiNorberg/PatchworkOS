#include "rsdt/rsdt.h"

#include "tty/tty.h"
#include "vmm/vmm.h"
#include "list/list.h"

#include <libc/string.h>

static List* tables;

void rsdt_init(Xsdt* xsdp)
{
    tty_start_message("RSDT initializing");

    SdtHeader* xsdt = (void*)xsdp->xsdtAddress;

    tables = list_new();

    uint64_t tableAmount = (xsdt->length - sizeof(SdtHeader)) / 8;
    for (uint64_t i = 0; i < tableAmount; i++)
    {
        SdtHeader* table = (SdtHeader*)*((void**)((uint64_t)xsdt + sizeof(SdtHeader) + i * 8));
        list_push(tables, vmm_request_address(table, 1, PAGE_FLAG_READ_WRITE));
    }

    tty_end_message(TTY_MESSAGE_OK);
}

SdtHeader* rsdt_lookup(const char* signature)
{
    ListEntry* entry = tables->first;
    while (entry != 0)
    {
        SdtHeader* table = entry->data;

        if (memcmp(table->signature, signature, 4) == 0)
        {
            return table;
        }

        entry = entry->next;
    }

    return 0;
}