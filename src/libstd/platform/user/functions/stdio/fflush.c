#include <stdio.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/file.h"

int fflush(FILE* stream)
{
    uint64_t result = 0;
    if (stream == NULL)
    {
        result = _files_flush();
    }
    else
    {
        _PLATFORM_MUTEX_ACQUIRE(&stream->mtx);
        result = _file_flush_buffer(stream);
        _PLATFORM_MUTEX_RELEASE(&stream->mtx);
    }

    return result == ERR ? EOF : 0;
}
