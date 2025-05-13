#include <stdlib.h>

#include "common/digits.h"

char* ulltoa(unsigned long long value, char* str, int base)
{
    if (base < 2 || base > 36)
    {
        *str = '\0';
        return str;
    }
    
    if (value == 0)
    {
        str[0] = '0';
        str[1] = '\0';
        return str;
    }
        
    char* ptr = str;
    char* startPtr = str;
    
    while (value > 0)
    {
        *ptr++ = _Digits[value % base];
        value /= base;
    }
    
    *ptr-- = '\0';
    
    char temp;
    while (startPtr < ptr)
    {
        temp = *startPtr;
        *startPtr++ = *ptr;
        *ptr-- = temp;
    }
    
    return str;
}
