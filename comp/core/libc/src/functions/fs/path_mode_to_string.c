#include "common/path_mode.h"

#include <string.h>

status_t path_mode_to_string(path_mode_t mode, char* out, uint64_t length, uint64_t* outLength)
{
    if (out == NULL || length == 0)
    {
        return ERR(VFS, INVAL);
    }

    uint64_t index = 0;
    for (uint64_t i = 0; i < ARRAY_SIZE(flags); i++)
    {
        if (mode & flags[i].mode)
        {
            uint64_t nameLength = strnlen_s(flags[i].name, MAX_NAME);
            if (index + nameLength + 1 >= length)
            {
                return ERR(VFS, NAMETOOLONG);
            }

            out[index] = ':';
            index++;

            memcpy(&out[index], flags[i].name, nameLength);
            index += nameLength;
        }
    }

    out[index] = '\0';
    if (outLength != NULL)
    {
        *outLength = index;
    }
    return OK;
}