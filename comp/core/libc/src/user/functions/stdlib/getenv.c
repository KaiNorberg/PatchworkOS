#include "user/common/threading.h"
#include <libc/io.h>
#include <stdlib.h>

char* getenv(const char* name)
{
    if (name == NULL || environ == NULL)
    {
        return NULL;
    }

    size_t len = strlen(name);
    for (char** env = environ; *env != NULL; env++)
    {
        if (strncmp(*env, name, len) == 0 && (*env)[len] == '=')
        {
            return &((*env)[len + 1]);
        }
    }

    return NULL;
}