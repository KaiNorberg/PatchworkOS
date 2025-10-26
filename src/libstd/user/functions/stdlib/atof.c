#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

// TODO: Finish this implementation, missing NAN, INF 0x/0X handling.

double atof(const char* nptr)
{
    double result = 0.0;
    double fraction = 0.0;
    double divisor = 1.0;
    int exponent = 0;
    bool isNegative = false;
    bool hasExponent = false;
    bool isExponentNegative = false;

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

    while (*nptr >= '0' && *nptr <= '9')
    {
        result = result * 10.0 + (*nptr - '0');
        nptr++;
    }

    if (*nptr == '.')
    {
        nptr++;

        while (*nptr >= '0' && *nptr <= '9')
        {
            fraction = fraction * 10.0 + (*nptr - '0');
            divisor *= 10.0;
            nptr++;
        }

        result += fraction / divisor;
    }

    if (*nptr == 'e' || *nptr == 'E')
    {
        nptr++;
        hasExponent = true;

        if (*nptr == '+')
        {
            nptr++;
        }
        else if (*nptr == '-')
        {
            nptr++;
            isExponentNegative = true;
        }

        while (*nptr >= '0' && *nptr <= '9')
        {
            exponent = exponent * 10 + (*nptr - '0');
            nptr++;
        }
    }

    if (hasExponent)
    {
        double power = 1.0;
        for (int i = 0; i < exponent; i++)
        {
            power *= 10.0;
        }

        if (isExponentNegative)
        {
            result /= power;
        }
        else
        {
            result *= power;
        }
    }

    return isNegative ? -result : result;
}
