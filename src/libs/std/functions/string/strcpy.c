#include <string.h>

char* strcpy(char* _RESTRICT dest, const char* _RESTRICT src)
{
    char* temp = dest;

    while ((*dest++ = *src++))
    {
    }

    return temp;
}