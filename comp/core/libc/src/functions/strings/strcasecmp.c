#include <ctype.h>
#include <strings.h>

int strcasecmp(const char* s1, const char* s2)
{
    unsigned char c1, c2;

    if (s1 == s2)
    {
        return 0;
    }

    do
    {
        c1 = tolower((unsigned char)*s1++);
        c2 = tolower((unsigned char)*s2++);

        if (c1 != c2)
        {
            return c1 - c2;
        }

        if (c1 == '\0')
        {
            return 0;
        }
    } while (1);
}
