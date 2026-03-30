#include <string.h>

char* strncat(char* _RESTRICT s1, const char* _RESTRICT s2, size_t n)
{
    char* rc = s1;

    while (*s1)
    {
        ++s1;
    }

    while (n && (*s1++ = *s2++))
    {
        --n;
    }

    if (n == 0)
    {
        *s1 = '\0';
    }

    return rc;
}
