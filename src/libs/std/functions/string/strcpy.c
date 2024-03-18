#include <string.h>

char* strcpy(char* __RESTRICT dest, const char* __RESTRICT src)
{
    char* temp = dest;

    while ((*dest++ = *src++));

    return temp;
}