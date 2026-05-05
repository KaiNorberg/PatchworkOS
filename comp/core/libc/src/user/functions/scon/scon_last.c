#include "scon_priv.h"

scon_item_t* scon_last(scon_item_t* list)
{
    if (list == NULL || list->type == SCON_ATOM)
    {
        return NULL;
    }
    return list->list.last;
}