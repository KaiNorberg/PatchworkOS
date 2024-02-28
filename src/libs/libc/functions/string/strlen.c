#include <libc/string.h>
#include <stddef.h>

size_t strlen(const char* str)
{
    size_t i = 0;

    while (str[i])
    {
        i++;
    }

    return i;
}