#include <stdint.h>
#include <string.h>

void* memset32(void* s, uint32_t c, size_t n)
{
    uint32_t* p = s;
    uint64_t c64 = ((uint64_t)c << 32) | c;

    while (((uintptr_t)p & 3) && n)
    {
        *p++ = c;
        n--;
    }

    while (n >= 16)
    {
        ((uint64_t*)p)[0] = c64;
        ((uint64_t*)p)[1] = c64;
        ((uint64_t*)p)[2] = c64;
        ((uint64_t*)p)[3] = c64;
        ((uint64_t*)p)[4] = c64;
        ((uint64_t*)p)[5] = c64;
        ((uint64_t*)p)[6] = c64;
        ((uint64_t*)p)[7] = c64;
        p += 16;
        n -= 16;
    }

    while (n >= 4)
    {
        ((uint64_t*)p)[0] = c64;
        ((uint64_t*)p)[1] = c64;
        p += 4;
        n -= 4;
    }

    while (n)
    {
        *p++ = c;
        n--;
    }

    return s;
}