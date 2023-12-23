#include <stdint.h>
#include <stddef.h>

#include "string.h"

char* strcpy(char* dest, const char* src)
{
    uint64_t len = strlen(src);
    for (int i = 0; i < len; i++)
    {
        dest[i] = src[i];
    }

    return dest;
}