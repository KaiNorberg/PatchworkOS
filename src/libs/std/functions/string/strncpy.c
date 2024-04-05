#include <string.h>

char* strncpy(char* _RESTRICT dest, const char* _RESTRICT src, size_t count)
{
    char* ret = dest;

    while (count && (*dest++ = *src++))
    {
        --count;
    }

    while (count-- > 1)
    {
        *dest++ = '\0';
    }

    return ret;
}