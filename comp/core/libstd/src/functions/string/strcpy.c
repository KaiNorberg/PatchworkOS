#include <string.h>

char* strcpy(char* _RESTRICT s1, const char* _RESTRICT s2)
{
    char* rc = s1;

    while ((*s1++ = *s2++))
    {
        /* EMPTY */
    }

    return rc;
}
