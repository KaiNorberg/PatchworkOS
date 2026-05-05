#include "scon_priv.h"

scon_item_t* scon_first(scon_item_t* list)
{
    if (list == NULL || list->type == SCON_ATOM)
    {
        return NULL;
    }
    return list->list.first;
}