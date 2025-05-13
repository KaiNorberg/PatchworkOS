#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "common/digits.h"

long long int atoll(const char* nptr)
{
    long long int result = 0;
    bool isNegative = false;

    while (isspace((unsigned char)*nptr))
    {
        nptr++;
    }

    if (*nptr == '+')
    {
        nptr++;
    }
    else if (*nptr == '-')
    {
        nptr++;
        isNegative = true;
    }

    const char* x;
    while ((x = (const char*)memchr(_Digits, tolower((unsigned char)*(nptr++)), 10)) != NULL)
    {
        result = result * 10 + (x - _Digits);
    }

    return isNegative ? -result : result;
}
