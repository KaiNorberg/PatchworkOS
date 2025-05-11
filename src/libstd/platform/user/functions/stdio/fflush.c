#include <stdio.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/file.h"

int fflush(FILE* stream)
{
    uint64_t result = 0;
    if (stream == NULL)
    {
        result = _FilesFlush();
    }
    else
    {
        _PLATFORM_MUTEX_ACQUIRE(&stream->mtx);
        result = _FileFlushBuffer(stream);
        _PLATFORM_MUTEX_RELEASE(&stream->mtx);
    }

    return result == ERR ? EOF : 0;
}
