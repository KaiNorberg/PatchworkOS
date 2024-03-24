#include <string.h>

char* strrchr(const char* str, int ch)
{
    size_t i = 0;

    while (str[i++])
    {
        /* EMPTY */
    }

    do
    {
        if (str[--i] == (char)ch)
        {
            return (char*)str + i;
        }
    } while (i);

    return NULL;
}