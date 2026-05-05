#include "common/path_mode.h"

#include <string.h>

status_t path_string_to_mode(const char* str, size_t length, path_mode_t* out)
{
    if (str == NULL || out == NULL)
    {
        return ERR(VFS, INVAL);
    }

    if (length == 0)
    {
        *out = PATH_MODE_NONE;
        return OK;
    }

    for (size_t i = 0; i < ARRAY_SIZE(flags); i++)
    {
        size_t len = strnlen_s(flags[i].name, MAX_NAME);
        if (len == length && strncmp(str, flags[i].name, length) == 0)
        {
            *out = flags[i].mode;
            return OK;
        }
    }

    path_mode_t combinedMode = PATH_MODE_NONE;
    for (size_t i = 0; i < length; i++)
    {
        if (str[i] < 0 || (uint8_t)str[i] >= INT8_MAX)
        {
            return ERR(VFS, INVALCHAR);
        }
        path_mode_t mode = shortFlags[(uint8_t)str[i]].mode;
        if (mode == PATH_MODE_NONE)
        {
            return ERR(VFS, INVALFLAG);
        }
        combinedMode |= mode;
    }

    *out = combinedMode;
    return OK;
}
