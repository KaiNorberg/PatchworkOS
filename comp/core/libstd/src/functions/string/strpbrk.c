#include <string.h>

char* strpbrk(const char* s1, const char* s2)
{
    const char* p1 = s1;
    const char* p2;

    while (*p1)
    {
        p2 = s2;

        while (*p2)
        {
            if (*p1 == *p2++)
            {
                return (char*)p1;
            }
        }

        ++p1;
    }

    return NULL;
}
