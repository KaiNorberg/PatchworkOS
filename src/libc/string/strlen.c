#include "libc/include/string.h"

size_t strlen(const char* str)
{
    char* strPtr = str;
    while (*strPtr != '\0')
    {
        strPtr++;
    }
    return strPtr - str;
}