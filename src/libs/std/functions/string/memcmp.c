#include <string.h>

int memcmp(const void* lhs, const void* rhs, size_t count)
{
    unsigned char* p1 = (unsigned char*)lhs;
    unsigned char* p2 = (unsigned char*)rhs;

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