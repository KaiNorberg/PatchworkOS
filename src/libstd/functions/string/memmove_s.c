#include "common/use_annex_k.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common/constraint_handler.h"

errno_t memmove_s(void* s1, rsize_t s1max, const void* s2, rsize_t n)
{
    uint8_t* d = (uint8_t*)s1;
    const uint8_t* s = (const uint8_t*)s2;
    
    if (s1 == NULL || s2 == NULL || s1max > RSIZE_MAX || n > RSIZE_MAX || n > s1max)
    {
        if (s1 != NULL && s1max <= RSIZE_MAX)
        {
            memset(s1, 0, s1max);
        }
        
        _constraintHandler(_CONSTRAINT_VIOLATION(EINVAL));
        return EINVAL;
    }
    
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
    
    return 0;
}
