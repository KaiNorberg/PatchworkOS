#include "argv.h"

#include "mem/heap.h"

#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

uint64_t argv_init(argv_t* argv, const char** src)
{
    if (src == NULL)
    {
        argv->buffer = heap_alloc(sizeof(const char*), HEAP_NONE);
        argv->size = sizeof(const char*);
        argv->amount = 1;

        argv->buffer[0] = NULL;
        return 0;
    }

    uint64_t argc = 0;
    while (src[argc] != NULL)
    {
        argc++;
    }

    uint64_t size = sizeof(const char*) * (argc + 1);
    for (uint64_t i = 0; i < argc; i++)
    {
        uint64_t strLen = strnlen_s(src[i], MAX_PATH + 1);
        if (strLen >= MAX_PATH + 1)
        {
            return ERR;
        }
        size += strLen + 1;
    }

    char** dest = heap_alloc(size, HEAP_NONE);
    if (dest == NULL)
    {
        return ERR;
    }

    char* strings = (char*)((uintptr_t)dest + sizeof(char*) * (argc + 1));
    for (uint64_t i = 0; i < argc; i++)
    {
        dest[i] = strings;
        strcpy(strings, src[i]);
        strings += strlen(src[i]) + 1;
    }
    dest[argc] = NULL;

    argv->buffer = dest;
    argv->size = size;
    argv->amount = argc;

    return 0;
}

void argv_deinit(argv_t* argv)
{
    heap_free(argv->buffer);
}
