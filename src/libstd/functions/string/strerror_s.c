#include "common/use_annex_k.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/constraint_handler.h"

errno_t strerror_s(char* s, rsize_t maxsize, errno_t errnum)
{
    size_t len = strerrorlen_s(errnum);

    if (s == NULL || maxsize > RSIZE_MAX || maxsize == 0)
    {
        _ConstraintHandler(_CONSTRAINT_VIOLATION(EINVAL));
        return EINVAL;
    }

    if (len < maxsize)
    {
        strcpy(s, strerror(errnum));
    }
    else
    {
        strncpy(s, strerror(errnum), maxsize - 1);

        if (maxsize > 3)
        {
            strcpy(&s[maxsize - 4], "...");
        }
        else
        {
            s[maxsize - 1] = '\0';
        }
    }

    return 0;
}
