#include "common/path_mode.h"

status_t path_string_to_posix(const char* str, size_t length, mode_t* out)
{
    if (str == NULL || out == NULL)
    {
        return ERR(VFS, INVAL);
    }

    path_mode_t mode = PATH_MODE_NONE;
    const char* p = str;
    const char* end = str + length;

    while (p < end)
    {
        while (p < end && (*p == ':' || *p == ' '))
        {
            p++;
        }

        if (p >= end || *p == '?')
        {
            break;
        }

        const char* token = p;
        while (p < end && *p != ':' && *p != '?' && *p != ' ')
        {
            p++;
        }

        path_mode_t m;
        status_t status = path_string_to_mode(token, p - token, &m);
        if (IS_ERR(status))
        {
            return status;
        }
        mode |= m;
    }

    *out = path_mode_to_posix(mode);
    return OK;
}