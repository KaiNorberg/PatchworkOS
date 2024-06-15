#include <immintrin.h>
#include <stdint.h>
#include <string.h>

void* memcpy(void* _RESTRICT dest, const void* _RESTRICT src, size_t count)
{
    uint8_t* d = dest;
    const uint8_t* s = src;

    while (((uintptr_t)d & 7) && count)
    {
        *d++ = *s++;
        count--;
    }

    while (count >= 64)
    {
        *(uint64_t*)(d + 0) = *(const uint64_t*)(s + 0);
        *(uint64_t*)(d + 8) = *(const uint64_t*)(s + 8);
        *(uint64_t*)(d + 16) = *(const uint64_t*)(s + 16);
        *(uint64_t*)(d + 24) = *(const uint64_t*)(s + 24);
        *(uint64_t*)(d + 32) = *(const uint64_t*)(s + 32);
        *(uint64_t*)(d + 40) = *(const uint64_t*)(s + 40);
        *(uint64_t*)(d + 48) = *(const uint64_t*)(s + 48);
        *(uint64_t*)(d + 56) = *(const uint64_t*)(s + 56);
        d += 64;
        s += 64;
        count -= 64;
    }

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
