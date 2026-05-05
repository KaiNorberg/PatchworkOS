#include <ctype.h>
#include <stdlib.h>

#include "scon_priv.h"

void scon_free(scon_t* scon)
{
    if (scon == NULL)
    {
        return;
    }

    scon_block_t* block;
    scon_block_t* temp;
    LIST_FOR_EACH_SAFE(block, temp, &scon->blocks, link)
    {
        for (size_t i = 0; i < block->used; i++)
        {
            scon_item_t* item = &block->items[i];
            if (item->type == SCON_ATOM && item->flags & SCON_ITEM_DYNAMIC)
            {
                dstr_deinit(&item->dstr);
            }
        }
        list_remove(&block->link);
        free(block);
    }

    scon->root = NULL;
    list_init(&scon->blocks);
    scon->freeList = NULL;
    free(scon);
}