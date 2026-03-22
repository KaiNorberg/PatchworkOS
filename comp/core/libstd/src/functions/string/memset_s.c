#include "common/use_annex_k.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common/constraint_handler.h"

errno_t memset_s(void* s, rsize_t smax, int c, rsize_t n)
{
    uint8_t* p = (uint8_t*)s;

    if (s == NULL || smax > RSIZE_MAX || n > RSIZE_MAX || n > smax)
    {
        if (s != NULL && smax <= RSIZE_MAX)
        {
            uint8_t* zeroP = (uint8_t*)s;
            rsize_t zeroN = smax;

            while (((uintptr_t)zeroP & 7) && zeroN)
            {
                *zeroP++ = 0;
                zeroN--;
            }

            while (zeroN >= 64)
            {
                *(uint64_t*)(zeroP + 0) = 0;
                *(uint64_t*)(zeroP + 8) = 0;
                *(uint64_t*)(zeroP + 16) = 0;
                *(uint64_t*)(zeroP + 24) = 0;
                *(uint64_t*)(zeroP + 32) = 0;
                *(uint64_t*)(zeroP + 40) = 0;
                *(uint64_t*)(zeroP + 48) = 0;
                *(uint64_t*)(zeroP + 56) = 0;
                zeroP += 64;
                zeroN -= 64;
            }

            while (zeroN >= 8)
            {
                *(uint64_t*)zeroP = 0;
                zeroP += 8;
                zeroN -= 8;
            }

            while (zeroN--)
            {
                *zeroP++ = 0;
            }
        }

        _constraintHandler(_CONSTRAINT_VIOLATION(EINVAL));
        return EINVAL;
    }

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

    return 0;
}