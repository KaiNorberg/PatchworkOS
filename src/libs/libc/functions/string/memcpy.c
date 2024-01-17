#include <string.h>

void* memcpy(void* LIBC_RESTRICT src, const void* LIBC_RESTRICT dest, size_t count)
{
    char* p1 = (char *)src;
    char* p2 = (char *)dest;

    while (count--)
    {
        *p2++ = *p1++;
    }

    return src;
}