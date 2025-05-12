#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

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
    const char* digits = "0123456789";
    while ((x = (const char*)memchr(digits, tolower((unsigned char)*(nptr++)), 10)) != NULL)
    {
        result = result * 10 + (x - digits);
    }

    return isNegative ? -result : result;
}
