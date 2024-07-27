#ifndef __EMBED__

#include <sys/io.h>
#include <stdlib.h>

uint64_t loaddir(dir_entry_t** out, const char* path)
{
    uint64_t amount = listdir(path, NULL, 0);
    if (amount == ERR)
    {
        return ERR;
    }

    *out = malloc(sizeof(dir_entry_t) * amount);
    if (*out == NULL)
    {
        return ERR;
    }

    if (listdir(path, *out, amount) == ERR)
    {
        free(*out);
        return ERR;
    }

    return amount;
}

#endif
