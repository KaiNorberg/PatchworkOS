#include <string.h>

char* strstr(const char* s1, const char* s2)
{
    const char* p1 = s1;
    const char* p2;

    while (*s1)
    {
        p2 = s2;

        while (*p2 && (*p1 == *p2))
        {
            ++p1;
            ++p2;
        }

        if (!*p2)
        {
            return (char*)s1;
        }

        ++s1;
        p1 = s1;
    }

    return NULL;
}
