#include <stdio.h>
#include <stdlib.h>

#include "platform/user/common/file.h"
#include "platform/user/common/syscalls.h"

int fgetc(FILE* stream)
{
    int result = EOF;

    _PLATFORM_MUTEX_ACQUIRE(&stream->mtx);

    if (_file_prepare_read(stream) != ERR)
    {
        if (_FILE_CHECK_AVAIL(stream) != ERR)
        {
            result = _FILE_GETC(stream);
        }
    }

    _PLATFORM_MUTEX_RELEASE(&stream->mtx);

    return result;
}
