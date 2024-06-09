#include <immintrin.h>
#include <stdint.h>
#include <string.h>

void* memcpy(void* _RESTRICT dest, const void* _RESTRICT src, size_t count)
{
    uint8_t* d = dest;
    const uint8_t* s = src;

    while (count >= 8)
    {
        *(uint64_t*)d = *(const uint64_t*)s;
        d += 8;
        s += 8;
        count -= 8;
    }

    while (count--)
    {
        *d++ = *s++;
    }

    return dest;
}