#include <stdio.h>

#include "user/common/file.h"

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

    mtx_lock(&stream->mtx);

    if (_file_prepare_read(stream) != _FAIL)
    {
        do
        {
            if (_FILE_CHECK_AVAIL(stream) == _FAIL)
            {
                /* In case of error / EOF before a character is read, this
                   will lead to a \0 be written anyway. Since the results
                   are "indeterminate" by definition, this does not hurt.
                */
                break;
            }
        } while (((*dest++ = _FILE_GETC(stream)) != '\n') && (--size > 0));
    }

    mtx_unlock(&stream->mtx);

    *dest = '\0';
    return (dest == s) ? NULL : s;
}
