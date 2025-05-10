#include "common/use_annex_k.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common/constraint_handler.h"

errno_t memcpy_s(void* _RESTRICT s1, rsize_t s1max, const void* _RESTRICT s2, rsize_t n)
{
    char* dest = (char*)s1;
    const char* src = (const char*)s2;

    if (s1 != NULL && s2 != NULL && s1max <= RSIZE_MAX && n <= RSIZE_MAX && n <= s1max)
    {
        while (n--)
        {
            if (dest == s2 || src == s1)
            {
                goto runtime_constraint_violation;
            }

            *dest++ = *src++;
        }

        return 0;
    }

runtime_constraint_violation:

    if (s1 != NULL && s1max <= RSIZE_MAX)
    {
        memset(s1, 0, s1max);
    }

    _ConstraintHandler(_CONSTRAINT_VIOLATION(EINVAL));
    return EINVAL;
}
