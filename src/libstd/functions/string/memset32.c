#include <stdint.h>
#include <string.h>

void* memset32(void* s, uint32_t c, size_t n)
{
    uint32_t* p = s;

    while (((uintptr_t)p & 3) && n)
    {
        *p++ = c;
        n--;
    }

    while (n >= 4)
    {
        p[0] = c;
        p[1] = c;
        p[2] = c;
        p[3] = c;
        p += 4;
        n -= 4;
    }

    while (n >= 1)
    {
        *p++ = c;
        n--;
    }

    return s;
}