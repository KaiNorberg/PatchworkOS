#include "utils.h"

#include "libc/include/string.h"

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
    { //Move to where representation ends
        ++p;
        shifter = shifter/base;
    }
    while(shifter);

    *p = '\0';
    do
    { //Move back, inserting digits as u go
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
