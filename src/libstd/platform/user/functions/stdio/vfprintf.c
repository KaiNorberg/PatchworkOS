#include <stdio.h>
#include <stdlib.h>

#include "common/print.h"
#include "platform/user/common/file.h"
#include "platform/user/common/syscalls.h"

int vfprintf(FILE* _RESTRICT stream, const char* _RESTRICT format, va_list arg)
{
    _FormatCtx_t ctx;
    ctx.base = 0;
    ctx.flags = 0;
    ctx.maxChars = SIZE_MAX;
    ctx.totalChars = 0;
    ctx.currentChars = 0;
    ctx.buffer = NULL;
    ctx.width = 0;
    ctx.precision = EOF;
    ctx.stream = stream;

    _PLATFORM_MUTEX_ACQUIRE(&stream->mtx);

    if (_FilePrepareWrite(stream) == ERR)
    {
        _PLATFORM_MUTEX_RELEASE(&stream->mtx);
        return EOF;
    }

    va_copy(ctx.arg, arg);

    while (*format != '\0')
    {
        const char* rc;

        if ((*format != '%') || ((rc = _Print(format, &ctx)) == format))
        {
            /* No conversion specifier, print verbatim */
            stream->buf[stream->bufIndex++] = *format;

            if ((stream->bufIndex == stream->bufSize) || ((stream->flags & _FILE_LINE_BUFFERED) && (*format == '\n')) ||
                (stream->flags & _FILE_UNBUFFERED))
            {
                if (_FileFlushBuffer(stream) == ERR)
                {
                    _PLATFORM_MUTEX_RELEASE(&stream->mtx);
                    return EOF;
                }
            }

            ++format;
            ctx.totalChars++;
        }
        else
        {
            /* Continue parsing after conversion specifier */
            format = rc;
        }
    }

    va_end(ctx.arg);
    _PLATFORM_MUTEX_RELEASE(&stream->mtx);
    return ctx.totalChars;
}
