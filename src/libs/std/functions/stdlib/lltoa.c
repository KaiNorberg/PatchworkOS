#include <stdlib.h>

#include "libs/std/internal/syscalls.h"

char* lltoa(long long number, char* str, int base)
{
    char* p = str;
    long long i = number;

    if (i < 0) 
    {
        *p++ = '-';
        i = -i;
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
        uint8_t digit = i % base;

        *--p = digit < 10 ? '0' + digit : 'A' + digit - 10;
        i = i/base;
    }
    while(i);

    return str;
}