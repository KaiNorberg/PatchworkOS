#include <stdio.h>
#include <stdlib.h>

#include "platform/user/common/file.h"
#include "platform/user/common/syscalls.h"

int fclose(struct FILE* stream)
{
    _PLATFORM_MUTEX_ACQUIRE(&stream->mtx);

    if (stream->flags & _FILE_WRITE)
    {
        if (_FileFlushBuffer(stream) == ERR)
        {
            _PLATFORM_MUTEX_RELEASE(&stream->mtx);
            return EOF;
        }
    }

    _PLATFORM_MUTEX_RELEASE(&stream->mtx);

    _FilesRemove(stream);
    _FileDeinit(stream);
    _FileFree(stream);
    return 0;
}
