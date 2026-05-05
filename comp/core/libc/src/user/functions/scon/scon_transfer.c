#include "scon_priv.h"

void scon_transfer(scon_t* dst, scon_t* src)
{
    *dst = *src;
    if (!list_is_empty(&src->blocks))
    {
        dst->blocks.head.next->prev = &dst->blocks.head;
        dst->blocks.head.prev->next = &dst->blocks.head;
    }
    else
    {
        list_init(&dst->blocks);
    }
    src->root = NULL;
    list_init(&src->blocks);
    src->freeList = NULL;
}