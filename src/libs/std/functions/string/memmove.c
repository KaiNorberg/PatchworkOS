#include <string.h>

void* memmove(void* dest, const void* src, size_t size)
{
    char* p1 = (char *)src;
    char* p2 = (char *)dest;

    if (p2 <= p1)
    {
        while (size--)
        {
            *p2++ = *p1++;
        }
    }
    else
    {
        p1 += size;
        p2 += size;

        while (size--)
        {
            *--p2 = *--p1;
        }
    }

    return dest;
}