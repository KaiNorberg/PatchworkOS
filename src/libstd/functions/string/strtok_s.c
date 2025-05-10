#include "common/use_annex_k.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "common/constraint_handler.h"

char* strtok_s(char* _RESTRICT s1, rsize_t* _RESTRICT s1max, const char* _RESTRICT s2, char** _RESTRICT ptr)
{
    const char* p = s2;

    if (s1max == NULL || s2 == NULL || ptr == NULL || (s1 == NULL && *ptr == NULL) || *s1max > RSIZE_MAX)
    {
        _ConstraintHandler(_CONSTRAINT_VIOLATION(EINVAL));
        return NULL;
    }

    if (s1 != NULL)
    {
        /* new string */
        *ptr = s1;
    }
    else
    {
        /* old string continued */
        if (*ptr == NULL)
        {
            /* No old string, no new string, nothing to do */
            return NULL;
        }

        s1 = *ptr;
    }

    /* skip leading s2 characters */
    while (*p && *s1)
    {
        if (*s1 == *p)
        {
            /* found separator; skip and start over */
            if (*s1max == 0)
            {
                _ConstraintHandler(_CONSTRAINT_VIOLATION(EINVAL));
                return NULL;
            }

            ++s1;
            --(*s1max);
            p = s2;
            continue;
        }

        ++p;
    }

    if (!*s1)
    {
        /* no more to parse */
        *ptr = s1;
        return NULL;
    }

    /* skipping non-s2 characters */
    *ptr = s1;

    while (**ptr)
    {
        p = s2;

        while (*p)
        {
            if (**ptr == *p++)
            {
                /* found separator; overwrite with '\0', position *ptr, return */
                if (*s1max == 0)
                {
                    _ConstraintHandler(_CONSTRAINT_VIOLATION(EINVAL));
                    return NULL;
                }

                --(*s1max);
                *((*ptr)++) = '\0';
                return s1;
            }
        }

        if (*s1max == 0)
        {
            _ConstraintHandler(_CONSTRAINT_VIOLATION(EINVAL));
            return NULL;
        }

        --(*s1max);
        ++(*ptr);
    }

    /* parsed to end of string */
    return s1;
}
