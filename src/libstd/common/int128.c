#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

int128_t __divti3(int128_t a, int128_t b)
{
    assert(b != 0 && "Division by zero in __divti3");

    int negative = 0;
    if (a < 0)
    {
        negative = !negative;
        a = -a;
    }
    if (b < 0)
    {
        negative = !negative;
        b = -b;
    }

    int128_t quotient = 0;
    int128_t temp = 1;
    int128_t denom = b;

    while (denom <= a)
    {
        denom <<= 1;
        temp <<= 1;
    }

    while (temp > 1)
    {
        denom >>= 1;
        temp >>= 1;

        if (a >= denom)
        {
            a -= denom;
            quotient += temp;
        }
    }

    return negative ? -quotient : quotient;
}

int128_t __modti3(int128_t a, int128_t b)
{
    assert(b != 0 && "Division by zero in __modti3");

    int negative = 0;
    if (a < 0)
    {
        negative = 1;
        a = -a;
    }
    if (b < 0)
    {
        b = -b;
    }

    int128_t denom = b;

    while (denom <= a)
    {
        denom <<= 1;
    }

    while (denom > b)
    {
        denom >>= 1;

        if (a >= denom)
        {
            a -= denom;
        }
    }

    return negative ? -a : a;
}