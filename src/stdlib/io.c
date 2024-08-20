#ifndef __EMBED__

#include <stdlib.h>
#include <sys/io.h>

dir_list_t* loaddir(const char* path)
{
    uint64_t amount = listdir(path, NULL, 0);
    if (amount == ERR)
    {
        return NULL;
    }

    dir_list_t* list = malloc(sizeof(dir_list_t) + sizeof(dir_entry_t) * amount);
    if (list == NULL)
    {
        return NULL;
    }

    list->amount = amount;
    if (listdir(path, list->entries, amount) == ERR)
    {
        free(list);
        return NULL;
    }

    return list;
}

#endif
