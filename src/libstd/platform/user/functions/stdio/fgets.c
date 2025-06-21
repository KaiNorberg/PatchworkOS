#include <stdio.h>

#include "platform/platform.h"
#include "platform/user/common/file.h"

char* fgets(char* _RESTRICT s, int size, FILE* _RESTRICT stream)
{
    char* dest = s;

    if (size == 0)
    {
        return NULL;
    }

    if (size == 1)
    {
        *s = '\0';
        return s;
    }

    _PLATFORM_MUTEX_ACQUIRE(&stream->mtx);

    if (_file_prepare_read(stream) != ERR)
    {
        do
        {
            if (_FILE_CHECK_AVAIL(stream) == ERR)
            {
                /* In case of error / EOF before a character is read, this
                   will lead to a \0 be written anyway. Since the results
                   are "indeterminate" by definition, this does not hurt.
                */
                break;
            }
        } while (((*dest++ = _FILE_GETC(stream)) != '\n') && (--size > 0));
    }

    _PLATFORM_MUTEX_RELEASE(&stream->mtx);

    *dest = '\0';
    return (dest == s) ? NULL : s;
}