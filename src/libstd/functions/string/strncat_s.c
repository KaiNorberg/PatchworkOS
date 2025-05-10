#include "common/use_annex_k.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common/constraint_handler.h"

errno_t strncat_s(char* _RESTRICT s1, rsize_t s1max, const char* _RESTRICT s2, rsize_t n)
{
    char* dest = s1;
    const char* src = s2;

    if (s1 != NULL && s2 != NULL && s1max <= RSIZE_MAX && n <= RSIZE_MAX && s1max != 0)
    {
        while (*dest)
        {
            if (--s1max == 0 || dest++ == s2)
            {
                goto runtime_constraint_violation;
            }
        }

        do
        {
            if (n-- == 0)
            {
                *dest = '\0';
                return 0;
            }

            if (s1max-- == 0 || dest == s2 || src == s1)
            {
                goto runtime_constraint_violation;
            }
        } while ((*dest++ = *src++));

        return 0;
    }

runtime_constraint_violation:

    if (s1 != NULL && s1max > 0 && s1max <= RSIZE_MAX)
    {
        s1[0] = '\0';
    }

    _ConstraintHandler(_CONSTRAINT_VIOLATION(EINVAL));
    return EINVAL;
}
