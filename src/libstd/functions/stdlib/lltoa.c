#include <stdlib.h>

char* lltoa(long long number, char* str, int base)
{
    char* p = str;
    long long i = number;

    if (i < 0)
    {
        *p++ = '-';
        i = -i;
    }

    long long shifter = i;
    do
    {
        ++p;
        shifter = shifter / base;
    } while (shifter);

    *p = '\0';
    do
    {
        char digit = i % base;

        *--p = digit < 10 ? '0' + digit : 'A' + digit - 10;
        i = i / base;
    } while (i);

    return str;
}
