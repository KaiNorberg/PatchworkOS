#include <stdlib.h>

char* ulltoa(unsigned long long number, char* str, int base)
{
    char* p = str;
    unsigned long long i = number;

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
