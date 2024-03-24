#include <string.h>

char* strchr(const char* str, int ch)
{
    do
    {
        if (*str == (char)ch)
        {
            return (char*)str;
        }
    } 
    while(*str++);

    return NULL;
}