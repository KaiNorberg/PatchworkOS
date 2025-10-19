#include <stdio.h>

#include "platform/user/common/file.h"

int fputs(const char* _RESTRICT s, FILE* _RESTRICT stream)
{
    mtx_lock(&stream->mtx);

    if (_file_prepare_write(stream) == ERR)
    {
        mtx_unlock(&stream->mtx);
        return EOF;
    }

    while (*s != '\0')
    {
        stream->buf[stream->bufIndex++] = *s;

        if ((stream->bufIndex == stream->bufSize) || ((stream->flags & _FILE_LINE_BUFFERED) && *s == '\n'))
        {
            if (_file_flush_buffer(stream) == ERR)
            {
                mtx_unlock(&stream->mtx);
                return EOF;
            }
        }

        ++s;
    }

    if (stream->flags & _FILE_UNBUFFERED)
    {
        if (_file_flush_buffer(stream) == ERR)
        {
            mtx_unlock(&stream->mtx);
            return EOF;
        }
    }

    mtx_unlock(&stream->mtx);

    return 0;
}
