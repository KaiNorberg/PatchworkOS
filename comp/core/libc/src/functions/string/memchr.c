#include <string.h>

void* memchr(const void* s, int c, size_t n)
{
    const unsigned char* p = (const unsigned char*)s;

    while (n--)
    {
        if (*p == (unsigned char)c)
        {
            return (void*)p;
        }

        ++p;
    }

    return NULL;
}
