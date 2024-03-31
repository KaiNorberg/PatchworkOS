#include "utils.h"

#include <string.h>

uint64_t stoi(const char* string) 
{
    uint64_t multiplier = 1;
    uint64_t result = 0;
    for (int64_t i = strlen(string) - 1; i >= 0; i--) 
    {
        result += multiplier * (string[i] - '0');
        multiplier *= 10;
    }
    return result;
}

uint64_t round_pow2(uint64_t number)
{
    number--;
    number |= number >> 1;
    number |= number >> 2;
    number |= number >> 4;
    number |= number >> 8;
    number |= number >> 16;
    number |= number >> 32;
    number++;

    return number;
}

uint64_t nearest_pow2_exponent(uint64_t number)
{
    if ((number & (number - 1)) == 0)
    {
        return __builtin_ctz(number);
    }

    uint64_t pow2 = round_pow2(number);

    uint64_t exponent = 0;
    while (pow2 > 1) 
    {
        pow2 >>= 1;
        exponent++;
    }

    return exponent;
}

uint64_t round_up(uint64_t number, uint64_t multiple)
{
    return ((number + multiple - 1) / multiple) * multiple;
}

uint64_t round_down(uint64_t number, uint64_t multiple)
{
    return (number / multiple) * multiple;
}