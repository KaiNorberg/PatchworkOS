#include "common/use_annex_k.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common/constraint_handler.h"

errno_t memmove_s(void* s1, rsize_t s1max, const void* s2, rsize_t n)
{
    char* dest = (char*)s1;
    const char* src = (const char*)s2;

    if (s1 == NULL || s2 == NULL || s1max > RSIZE_MAX || n > RSIZE_MAX || n > s1max)
    {
        if (s1 != NULL && s1max <= RSIZE_MAX)
        {
            memset(s1, 0, s1max);
        }

        _constraintHandler(_CONSTRAINT_VIOLATION(EINVAL));
        return EINVAL;
    }

    while (n)
    {
        if (dest == s2 || src == s1)
        {
            src += n;
            dest += n;

            while (n--)
            {
                *--dest = *--src;
            }

            return 0;
        }

        *dest++ = *src++;
        --n;
    }

    return 0;
}
