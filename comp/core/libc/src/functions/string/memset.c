#include <stdint.h>
#include <string.h>

void* memset(void* s, int c, size_t n)
{
    uint8_t* p = s;

    uint8_t ch8 = (uint8_t)c;
    uint64_t ch64 = ((uint64_t)ch8) | ((uint64_t)ch8 << 8) | ((uint64_t)ch8 << 16) | ((uint64_t)ch8 << 24) |
        ((uint64_t)ch8 << 32) | ((uint64_t)ch8 << 40) | ((uint64_t)ch8 << 48) | ((uint64_t)ch8 << 56);

    while (((uintptr_t)p & 7) && n)
    {
        *p++ = ch8;
        n--;
    }

    while (n >= 64)
    {
        *(uint64_t*)(p + 0) = ch64;
        *(uint64_t*)(p + 8) = ch64;
        *(uint64_t*)(p + 16) = ch64;
        *(uint64_t*)(p + 24) = ch64;
        *(uint64_t*)(p + 32) = ch64;
        *(uint64_t*)(p + 40) = ch64;
        *(uint64_t*)(p + 48) = ch64;
        *(uint64_t*)(p + 56) = ch64;
        p += 64;
        n -= 64;
    }

    while (n >= 8)
    {
        *(uint64_t*)p = ch64;
        p += 8;
        n -= 8;
    }

    while (n--)
    {
        *p++ = ch8;
    }

    return s;
}
