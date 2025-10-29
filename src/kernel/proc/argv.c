#include <kernel/proc/argv.h>

#include <kernel/config.h>

#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

uint64_t argv_init(argv_t* argv, const char** src)
{
    if (src == NULL || src[0] == NULL)
    {
        argv->buffer = argv->empty;
        argv->size = sizeof(const char*);
        argv->amount = 1;

        argv->buffer[0] = NULL;
        return 0;
    }

    uint64_t argc = 0;
    while (src[argc] != NULL && argc < CONFIG_MAX_ARGC)
    {
        argc++;
    }

    if (argc == CONFIG_MAX_ARGC)
    {
        return ERR;
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

    char** dest = malloc(size);
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
    if (argv->buffer == argv->empty)
    {
        return;
    }
    free(argv->buffer);
}

const char* argv_get_strings(const argv_t* argv, uint64_t* length)
{
    if (argv->amount == 0)
    {
        *length = 0;
        return NULL;
    }

    const char* first = (const char*)((uintptr_t)argv->buffer + sizeof(char*) * (argv->amount + 1));
    *length = argv->size - (sizeof(char*) * (argv->amount + 1));
    return first;
}
