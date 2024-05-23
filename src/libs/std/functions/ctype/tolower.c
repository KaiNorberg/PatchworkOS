#include <ctype.h>

int tolower(int c) 
{
    if (c >= 'A' && c <= 'Z') 
    {
        return c + ('a' - 'A');
    }
    
    return c;
}