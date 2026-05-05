#include "scon_priv.h"

scon_item_t* scon_get(scon_item_t* list, size_t n)
{
    if (list == NULL || list->type != SCON_LIST)
    {
        return NULL;
    }

    scon_item_t* item = scon_first(list);
    for (size_t i = 0; i < n && item != NULL; i++)
    {
        item = scon_next(item);
    }

    return item;
}