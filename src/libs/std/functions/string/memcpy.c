#include <string.h>
#include <stdint.h>
#include <immintrin.h>

void* memcpy(void* _RESTRICT dest, const void* _RESTRICT src, size_t count)
{	
    unsigned char *d = dest;
    const unsigned char *s = src;

    while (count >= 8) 
    {
        *(uint64_t *)d = *(const uint64_t *)s;
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