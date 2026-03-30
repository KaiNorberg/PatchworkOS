#include "common/use_annex_k.h"
#include <string.h>

char* strtok(char* _RESTRICT s1, const char* _RESTRICT s2)
{
    static char* tmp = NULL;
    static rsize_t max;

    if (s1 != NULL)
    {
        tmp = s1;
        max = strlen(tmp);
    }

    return strtok_s(s1, &max, s2, &tmp);
}
