#include <stdio.h>
#include <stdlib.h>

#include "platform/user/common/file.h"
#include "platform/user/common/syscalls.h"

int fputc(int c, FILE* stream)
{
    _PLATFORM_MUTEX_ACQUIRE(&stream->mtx);

    if (_file_prepare_write(stream) == ERR)
    {
        _PLATFORM_MUTEX_RELEASE(&stream->mtx);
        return EOF;
    }

    stream->buf[stream->bufIndex++] = (char)c;
    if ((stream->bufIndex == stream->bufSize) || ((stream->flags & _FILE_LINE_BUFFERED) && ((char)c == '\n')) ||
        (stream->flags & _FILE_UNBUFFERED))
    {
        // buffer filled, unbuffered stream, or end-of-line.
        c = (_file_flush_buffer(stream) != ERR) ? c : EOF;
    }

    _PLATFORM_MUTEX_RELEASE(&stream->mtx);

    return c;
}
