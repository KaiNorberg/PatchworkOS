#include <string.h>

char* strcat(char* _RESTRICT dest, const char* _RESTRICT src)
{
    char* ret = dest;

    if (*dest)
    {
        while (*++dest)
        {
            /* EMPTY */
        }
    }

    while ((*dest++ = *src++))
    {
        /* EMPTY */
    }

    return ret;
}