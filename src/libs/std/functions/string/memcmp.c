#include <string.h>

int memcmp(const void* a, const void* b, size_t count)
{
    unsigned char* p1 = (unsigned char*)a;
    unsigned char* p2 = (unsigned char*)b;

    while (count--)
    {
        if (*p1 != *p2)
        {
            return *p1 - *p2;
        }

        ++p1;
        ++p2;
    }

    return 0;
}