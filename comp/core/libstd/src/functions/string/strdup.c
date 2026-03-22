#include <stdint.h>
#include <stdlib.h>
#include <string.h>

char* strdup(const char* src)
{
    uint64_t len = strlen(src);
    char* str = malloc(len + 1);
    if (str == NULL)
    {
        return NULL;
    }

    memcpy(str, src, len);
    str[len] = '\0';
    return str;
}
