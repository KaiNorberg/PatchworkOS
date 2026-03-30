#include <string.h>

char* strrchr(const char* s, int c)
{
    size_t i = 0;

    while (s[i++])
    {
        /* EMPTY */
    }

    do
    {
        if (s[--i] == (char)c)
        {
            return (char*)s + i;
        }
    } while (i);

    return NULL;
}
