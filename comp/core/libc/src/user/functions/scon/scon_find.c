#include "scon_priv.h"

scon_item_t* scon_find(scon_item_t* list, const char* name)
{
    if (list == NULL || list->type != SCON_LIST)
    {
        return NULL;
    }

    scon_item_t* first = scon_first(list);
    while (first != NULL)
    {
        if (first->type == SCON_LIST)
        {
            scon_item_t* head = scon_first(first);
            if (head != NULL && head->type == SCON_ATOM && scon_atom_eq(head, name))
            {
                return first;
            }
        }
        first = scon_next(first);
    }

    return NULL;
}