#include <stdint.h>
#include <string.h>

void* memmove(void* s1, const void* s2, size_t n)
{
    uint8_t* d = s1;
    const uint8_t* s = s2;

    if (d > s && d < (s + n))
    {
        d += n;
        s += n;

        while ((((uintptr_t)d & 7) != ((uintptr_t)s & 7)) && n)
        {
            *(--d) = *(--s);
            n--;
        }

        while (n >= 64)
        {
            d -= 64;
            s -= 64;
            *(uint64_t*)(d + 56) = *(const uint64_t*)(s + 56);
            *(uint64_t*)(d + 48) = *(const uint64_t*)(s + 48);
            *(uint64_t*)(d + 40) = *(const uint64_t*)(s + 40);
            *(uint64_t*)(d + 32) = *(const uint64_t*)(s + 32);
            *(uint64_t*)(d + 24) = *(const uint64_t*)(s + 24);
            *(uint64_t*)(d + 16) = *(const uint64_t*)(s + 16);
            *(uint64_t*)(d + 8) = *(const uint64_t*)(s + 8);
            *(uint64_t*)(d + 0) = *(const uint64_t*)(s + 0);
            n -= 64;
        }

        while (n >= 8)
        {
            d -= 8;
            s -= 8;
            *(uint64_t*)d = *(const uint64_t*)s;
            n -= 8;
        }

        while (n--)
        {
            *(--d) = *(--s);
        }
    }
    else
    {
        while (((uintptr_t)d & 7) && n)
        {
            *d++ = *s++;
            n--;
        }

        while (n >= 64)
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
            n -= 64;
        }

        while (n >= 8)
        {
            *(uint64_t*)d = *(const uint64_t*)s;
            d += 8;
            s += 8;
            n -= 8;
        }

        while (n--)
        {
            *d++ = *s++;
        }
    }

    return s1;
}
