#include <ctype.h>
#include <stdlib.h>
#include <sys/lisc.h>

void lisc_deinit(lisc_t* lisc)
{
    if (lisc == NULL)
    {
        return;
    }

    if (lisc->items != lisc->small)
    {
        free(lisc->items);
    }

    lisc->items = lisc->small;
    lisc->count = 0;
    lisc->capacity = LISC_SMALL_MAX;
}