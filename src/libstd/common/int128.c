#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct
{
    uint128_t raw;
    union {
        struct
        {
            uint64_t low;
            uint64_t high;
        };
    };
} uint128_split_t;

uint64_t __div128_64(uint64_t high, uint64_t low, uint64_t divisor, uint64_t* remainder)
{
    uint64_t result;
    __asm__("divq %[v]" : "=a"(result), "=d"(*remainder) : [v] "r"(divisor), "a"(low), "d"(high));
    return result;
}

uint128_t __udivmodti4(uint128_t a, uint128_t b, uint128_t* c)
{
    uint128_split_t dividend = {.raw = a};
    uint128_split_t divisor = {.raw = b};
    uint128_split_t quotient = {0};
    uint128_split_t remainder = {0};

    if (divisor.raw > dividend.raw)
    {
        if (c != NULL)
        {
            *c = dividend.raw;
        }
        return 0;
    }

    uint32_t shift = __builtin_clzll(divisor.high) - __builtin_clzll(dividend.high);
    divisor.raw <<= shift;

    for (uint32_t i = 0; i <= shift; i++)
    {
        uint128_t bit = (uint128_t)(divisor.raw - dividend.raw - 1) >> 127;
        quotient.low |= bit & 1;
        dividend.raw -= divisor.raw & bit;
        divisor.raw >>= 1;
    }

    if (c != NULL)
    {
        *c = dividend.raw;
    }
    return quotient.raw;
}

int128_t __divti3(int128_t a, int128_t b)
{
    assert(b != 0 && "Division by zero in __divti3");

    int128_t sign = 1;
    if (a < 0)
    {
        sign = -sign;
        a = -a;
    }
    if (b < 0)
    {
        sign = -sign;
        b = -b;
    }
    uint128_t ua = (uint128_t)a;
    uint128_t ub = (uint128_t)b;
    uint128_t uq = __udivmodti4(ua, ub, NULL);
    return (int128_t)uq * sign;
}