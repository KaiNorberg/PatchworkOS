#include "common/use_annex_k.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common/constraint_handler.h"

errno_t memset_s(void* s, rsize_t smax, int c, rsize_t n)
{
    unsigned char* p = (unsigned char*)s;

    if (s == NULL || smax > RSIZE_MAX || n > RSIZE_MAX || n > smax)
    {
        if (s != NULL && smax <= RSIZE_MAX)
        {
            memset(s, c, smax);
        }

        _constraintHandler(_CONSTRAINT_VIOLATION(EINVAL));
        return EINVAL;
    }

    while (n--)
    {
        *p++ = (unsigned char)c;
    }

    return 0;
}
