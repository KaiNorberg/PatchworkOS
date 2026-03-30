#include <ctype.h>
#include <libc/scon.h>
#include <stdlib.h>

void scon_deinit(scon_t* scon)
{
    if (scon == NULL)
    {
        return;
    }

    if (scon->items != scon->small)
    {
        free(scon->items);
    }

    scon->items = scon->small;
    scon->count = 0;
    scon->capacity = SCON_SMALL_MAX;
}