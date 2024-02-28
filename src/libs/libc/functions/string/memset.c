#include <libc/string.h>
#include <stddef.h>

void* memset(void* dest, int ch, size_t count)
{
    unsigned char* p = (unsigned char*)dest;

    while (count--)
    {
        *p++ = (unsigned char)ch;
    }

    return dest;
}