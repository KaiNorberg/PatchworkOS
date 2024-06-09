#include <string.h>

void* memmove(void* dest, const void* src, size_t count)
{
    char* p1 = (char*)src;
    char* p2 = (char*)dest;

    if (p2 <= p1)
    {
        while (count--)
        {
            *p2++ = *p1++;
        }
    }
    else
    {
        p1 += count;
        p2 += count;

        while (count--)
        {
            *--p2 = *--p1;
        }
    }

    return dest;
}