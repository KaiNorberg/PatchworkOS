#include "utils.h"

#include "string/string.h"

char* itoa(uint64_t i, char b[], uint8_t base)
{
    char const digit[] = "0123456789ABCDEF";
    char* p = b;

    if (i<0)
    {
        *p++ = '-';
        i *= -1;
    }

    uint64_t shifter = i;
    do
    {
        ++p;
        shifter = shifter/base;
    }
    while(shifter);

    *p = '\0';
    do
    {
        *--p = digit[i%base];
        i = i/base;
    }
    while(i);

    return b;
}

uint64_t stoi(const char* string) 
{
    uint64_t multiplier = 1, result = 0;
    for (uint64_t i = strlen(string) - 1; i >= 0; i--) 
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