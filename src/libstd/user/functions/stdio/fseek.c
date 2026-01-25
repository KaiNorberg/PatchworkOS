#include <stdio.h>
#include <sys/fs.h>

#include "user/common/file.h"

int fseek(FILE* stream, long offset, int whence)
{
    mtx_lock(&stream->mtx);

    if (stream->flags & _FILE_WRITE)
    {
        if (_file_flush_buffer(stream) == _FAIL)
        {
            mtx_unlock(&stream->mtx);
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

    uint64_t result = (_file_seek(stream, offset, whence) != _FAIL) ? 0 : EOF;
    mtx_unlock(&stream->mtx);
    return result;
}
