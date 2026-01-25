#include <stdio.h>
#include <stdlib.h>

#include "user/common/file.h"
#include "user/common/syscalls.h"

int fputc(int c, FILE* stream)
{
    mtx_lock(&stream->mtx);

    if (_file_prepare_write(stream) == _FAIL)
    {
        mtx_unlock(&stream->mtx);
        return EOF;
    }

    stream->buf[stream->bufIndex++] = (char)c;
    if ((stream->bufIndex == stream->bufSize) || ((stream->flags & _FILE_LINE_BUFFERED) && ((char)c == '\n')) ||
        (stream->flags & _FILE_UNBUFFERED))
    {
        // buffer filled, unbuffered stream, or end-of-line.
        c = (_file_flush_buffer(stream) != _FAIL) ? c : EOF;
    }

    mtx_unlock(&stream->mtx);

    return c;
}
