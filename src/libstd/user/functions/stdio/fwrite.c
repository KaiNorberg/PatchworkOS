#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "user/common/file.h"
#include "user/common/syscalls.h"

size_t fwrite(const void* _RESTRICT ptr, size_t size, size_t nmemb, FILE* _RESTRICT stream)
{
    mtx_lock(&stream->mtx);

    if (_file_prepare_write(stream) == _FAIL)
    {
        mtx_unlock(&stream->mtx);
        return 0;
    }

    uint64_t newLineOffset = 0;
    uint64_t n = 0;
    for (; n < nmemb; n++)
    {
        for (uint64_t i = 0; i < size; i++)
        {
            /// @todo For better performance, write block-wise, not byte-wise.
            uint8_t byte = ((uint8_t*)ptr)[n * size + i];
            stream->buf[stream->bufIndex++] = byte;

            if (byte == '\n')
            {
                // Remember last newline, in case we have to do a partial line-buffered flush
                newLineOffset = stream->bufIndex;
            }

            if (stream->bufIndex == stream->bufSize)
            {
                if (_file_flush_buffer(stream) == _FAIL)
                {
                    mtx_unlock(&stream->mtx);
                    return n;
                }

                newLineOffset = 0;
            }
        }
    }

    if (stream->flags & _FILE_UNBUFFERED)
    {
        if (_file_flush_buffer(stream) == _FAIL)
        {
            /* We are in a pinch here. We have an error, which requires a
               return value < nmemb. On the other hand, all objects have
               been written to buffer, which means all the caller had to
               do was removing the error cause, and re-flush the stream...
               Catch 22. We'll return a value one short, to indicate the
               error, and can't really do anything about the inconsistency.
            */
            mtx_unlock(&stream->mtx);
            return n - 1;
        }
    }
    else if (stream->flags & _FILE_LINE_BUFFERED)
    {
        if (newLineOffset > 0)
        {
            size_t bufIndex = stream->bufIndex;
            stream->bufIndex = newLineOffset;

            if (_file_flush_buffer(stream) == _FAIL)
            {
                /* See comment above. */
                stream->bufIndex = bufIndex;
                mtx_unlock(&stream->mtx);
                return n - 1;
            }

            stream->bufIndex = bufIndex - newLineOffset;
            memmove(stream->buf, stream->buf + newLineOffset, stream->bufIndex);
        }
    }

    mtx_unlock(&stream->mtx);
    return n;
}
