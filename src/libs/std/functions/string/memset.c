#include <stdint.h>
#include <string.h>

void* memset(void* dest, int ch, size_t count)
{
    uint8_t* p = dest;

    uint8_t ch8 = (uint8_t)ch;
    uint64_t ch64 = ((uint64_t)ch8) | ((uint64_t)ch8 << 8) | ((uint64_t)ch8 << 16) | ((uint64_t)ch8 << 24) |
        ((uint64_t)ch8 << 32) | ((uint64_t)ch8 << 40) | ((uint64_t)ch8 << 48) | ((uint64_t)ch8 << 56);

    while (((uintptr_t)p & 7) && count)
    {
        *p++ = ch8;
        count--;
    }

    while (count >= 64)
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
        count -= 64;
    }

    while (count >= 8)
    {
        *(uint64_t*)p = ch64;
        p += 8;
        count -= 8;
    }

    while (count--)
    {
        *p++ = ch8;
    }

    return dest;
}
