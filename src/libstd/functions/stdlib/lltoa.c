#include <stdlib.h>
#include <stdbool.h>

#include "common/digits.h"

char* lltoa(long long int value, char* str, int base)
{
    if (value == 0)
    {
        str[0] = '0';
        str[1] = '\0';
        return str;
    }
    
    bool isNegative = false;
    if (value < 0)
    {
        isNegative = true;
        value = -value;
    }
    
    int i = 0;
    while (value != 0)
    {
        int remainder = value % base;
        str[i++] = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
        value = value / base;
    }
    
    if (isNegative)
    {
        str[i++] = '-';
    }
    
    str[i] = '\0';
    
    int start = 0;
    int end = i - 1;
    while (start < end)
    {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
    
    return str;
}
