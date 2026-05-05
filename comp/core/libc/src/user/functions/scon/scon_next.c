#include "scon_priv.h"

scon_item_t* scon_next(scon_item_t* entry)
{
    if (entry == NULL)
    {
        return NULL;
    }
    return entry->next;
}