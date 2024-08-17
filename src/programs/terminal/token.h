#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/io.h>

static const char* token_first(const char* command)
{
    return command;
}

static const char* token_next(const char* token)
{
    char* next = strchr(token, ' ');
    if (next == NULL)
    {
        return NULL;
    }

    while (1)
    {
        next++;
        if (*next == '\0')
        {
            return NULL;
        }
        else if (*next != ' ')
        {
            return next;
        }
    }
}

static bool token_equal(const char* a, const char* b)
{
    for (uint64_t i = 0; i < MAX_PATH; i++)
    {
        if (a[i] == '\0' || a[i] == ' ')
        {
            return b[i] == '\0' || b[i] == ' ';
        }
        if (a[i] != b[i])
        {
            return false;
        }
    }

    return false;
}

static void token_copy(char* dest, const char* src)
{
    for (uint64_t i = 0; i < MAX_NAME - 1; i++)
    {
        if (src[i] == '\0' || src[i] == ' ')
        {
            dest[i] = '\0';
            return;
        }
        else
        {
            dest[i] = src[i];
        }
    }
    dest[MAX_NAME - 1] = '\0';
}
