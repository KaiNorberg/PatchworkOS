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

uint64_t round_up(uint64_t number, uint64_t multiple)
{
    return ((number + multiple - 1) / multiple) * multiple;
}

uint64_t round_down(uint64_t number, uint64_t multiple)
{
    return (number / multiple) * multiple;
}