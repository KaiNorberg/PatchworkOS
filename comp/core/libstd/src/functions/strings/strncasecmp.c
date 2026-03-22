#include <ctype.h>
#include <strings.h>

int strncasecmp(const char* s1, const char* s2, size_t n)
{
    unsigned char c1, c2;

    if (s1 == s2 || n == 0)
    {
        return 0;
    }

    do
    {
        c1 = tolower((unsigned char)*s1++);
        c2 = tolower((unsigned char)*s2++);
        n--;

        if (c1 != c2)
        {
            return c1 - c2;
        }

        if (c1 == '\0' || n == 0)
        {
            return 0;
        }
    } while (1);
}
