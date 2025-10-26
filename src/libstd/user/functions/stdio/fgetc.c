#include <stdio.h>
#include <stdlib.h>

#include "user/common/file.h"
#include "user/common/syscalls.h"

int fgetc(FILE* stream)
{
    int result = EOF;

    mtx_lock(&stream->mtx);

    if (_file_prepare_read(stream) != ERR)
    {
        if (_FILE_CHECK_AVAIL(stream) != ERR)
        {
            result = _FILE_GETC(stream);
        }
    }

    mtx_unlock(&stream->mtx);

    return result;
}
