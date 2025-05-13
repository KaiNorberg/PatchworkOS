#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "common/digits.h"

static const char* _StrtollPrelim(const char* p, char* sign, int* base)
{
    /* skipping leading whitespace */
    while (isspace((unsigned char)*p))
    {
        ++p;
    }

    /* determining / skipping sign */
    if (*p != '+' && *p != '-')
    {
        *sign = '+';
    }
    else
    {
        *sign = *(p++);
    }

    /* determining base */
    if (*p == '0')
    {
        ++p;

        if ((*base == 0 || *base == 16) && (*p == 'x' || *p == 'X'))
        {
            *base = 16;
            ++p;

            /* catching a border case here: "0x" followed by a non-digit should
               be parsed as the unprefixed zero.
               We have to "rewind" the parsing; having the base set to 16 if it
               was zero previously does not hurt, as the result is zero anyway.
            */
            if (memchr(_Digits, tolower((unsigned char)*p), *base) == NULL)
            {
                p -= 2;
            }
        }
        else if (*base == 0)
        {
            *base = 8;
            /* back up one digit, so that a plain zero is decoded correctly
               (and endptr is set correctly as well).
               (2019-01-15, Giovanni Mascellani)
            */
            --p;
        }
        else
        {
            --p;
        }
    }
    else if (!*base)
    {
        *base = 10;
    }

    return ((*base >= 2) && (*base <= 36)) ? p : NULL;
}

static uintmax_t _StrtollMain(const char** p, unsigned int base, uintmax_t error, uintmax_t limval, int limdigit,
    char* sign)
{
    uintmax_t rc = 0;
    int digit = -1;
    const char* x;

    while ((x = (const char*)memchr(_Digits, tolower((unsigned char)**p), base)) != NULL)
    {
        digit = x - _Digits;

        if ((rc < limval) || ((rc == limval) && (digit <= limdigit)))
        {
            rc = rc * base + (unsigned)digit;
            ++(*p);
        }
        else
        {
            errno = ERANGE;

            /* TODO: Only if endptr != NULL - but do we really want *another* parameter? */
            /* TODO: Earlier version was missing tolower() here but was not caught by tests */
            while (memchr(_Digits, tolower((unsigned char)**p), base) != NULL)
            {
                ++(*p);
            }

            /* TODO: This is ugly, but keeps caller from negating the error value */
            *sign = '+';
            return error;
        }
    }

    if (digit == -1)
    {
        *p = NULL;
        return 0;
    }

    return rc;
}

long long int strtoll(const char* s, char** endptr, int base)
{
    long long int rc;
    char sign = '+';
    const char* p = _StrtollPrelim(s, &sign, &base);

    if (base < 2 || base > 36)
    {
        return 0;
    }

    if (sign == '+')
    {
        rc = (long long int)_StrtollMain(&p, (unsigned)base, (uintmax_t)LLONG_MAX, (uintmax_t)(LLONG_MAX / base),
            (int)(LLONG_MAX % base), &sign);
    }
    else
    {
        rc = (long long int)_StrtollMain(&p, (unsigned)base, (uintmax_t)LLONG_MIN, (uintmax_t)(LLONG_MIN / -base),
            (int)(-(LLONG_MIN % base)), &sign);
    }

    if (endptr != NULL)
    {
        *endptr = (p != NULL) ? (char*)p : (char*)s;
    }

    return (sign == '+') ? rc : -rc;
}