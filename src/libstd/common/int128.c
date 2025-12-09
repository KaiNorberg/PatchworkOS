#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// Based upon the this lovely article by Danila Kutenin
// https://danlark.org/2020/06/14/128-bit-division/

typedef union {
    uint128_t raw;
    struct
    {
        uint64_t low;
        uint64_t high;
    };
} uint128_split_t;

static inline int32_t __distance(uint128_split_t a, uint128_split_t b)
{
    int32_t clzA = a.high != 0 ? __builtin_clzll(a.high) : 64 + __builtin_clzll(a.low);
    int32_t clzB = b.high != 0 ? __builtin_clzll(b.high) : 64 + __builtin_clzll(b.low);
    return clzB - clzA;
}

static inline uint64_t __div128_64(uint64_t high, uint64_t low, uint64_t divisor, uint64_t* remainder)
{
    uint64_t result;
    __asm__("divq %[v]" : "=a"(result), "=d"(*remainder) : [v] "r"(divisor), "a"(low), "d"(high));
    return result;
}

uint128_t __udivmodti4(uint128_t a, uint128_t b, uint128_t* c)
{
    uint128_split_t dividend = *(uint128_split_t*)&a;
    uint128_split_t divisor = *(uint128_split_t*)&b;
    uint128_split_t remainder = {0};

    if (divisor.raw > dividend.raw)
    {
        remainder = dividend;
        if (c != NULL)
        {
            *c = remainder.raw;
        }
        return 0;
    }

    if (divisor.high == 0)
    {
        if (dividend.high < divisor.raw)
        {
            uint64_t quotient = __div128_64(dividend.high, dividend.low, divisor.low, &remainder.low);
            if (c != NULL)
            {
                *c = remainder.raw;
            }
            return quotient;
        }

        uint128_split_t quotient;
        quotient.high = __div128_64(0, dividend.high, divisor.low, &dividend.high);
        quotient.low = __div128_64(dividend.high, dividend.low, divisor.low, &remainder.low);
        if (c != NULL)
        {
            *c = remainder.raw;
        }
        return quotient.raw;
    }

    int32_t shift = __distance(dividend, divisor);
    divisor.raw <<= shift;
    uint128_split_t quotient = {0};
    for (int32_t i = 0; i <= shift; i++)
    {
        quotient.low <<= (uint128_t)1;
        const int128_t bit = (int128_t)(divisor.raw - dividend.raw - 1) >> (int128_t)127;
        quotient.low |= bit & (int128_t)1;
        dividend.raw -= divisor.raw & bit;
        divisor.raw >>= (uint128_t)1;
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