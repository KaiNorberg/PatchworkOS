#include <stdint.h>
#include <string.h>

size_t strnlen_s(const char* s, size_t maxsize)
{
    if (s == NULL)
    {
        return 0;
    }

    for (size_t i = 0; i < maxsize; i++)
    {
        if (s[i] == '\0')
        {
            return i;
        }
    }

    return maxsize;
}
