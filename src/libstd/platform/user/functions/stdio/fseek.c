#include <stdio.h>
#include <sys/io.h>

#include "platform/user/common/file.h"

int fseek(FILE* stream, long offset, int whence)
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

    stream->flags &= ~_FILE_EOF;

    if (stream->flags & _FILE_RW)
    {
        stream->flags &= ~(_FILE_READ | _FILE_WRITE);
    }

    if (whence == SEEK_CUR)
    {
        offset -= (((int)stream->bufEnd - (int)stream->bufIndex) + stream->ungetIndex);
    }

    uint64_t result = (_FileSeek(stream, offset, whence) != ERR) ? 0 : EOF;
    _PLATFORM_MUTEX_RELEASE(&stream->mtx);
    return result;
}
