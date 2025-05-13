#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

#include "common/scan.h"
#include "platform/user/common/file.h"

int vfscanf(FILE* _RESTRICT stream, const char* _RESTRICT format, va_list arg)
{
    /* TODO: This function should interpret format as multibyte characters.  */
    _FormatCtx_t ctx;
    ctx.base = 0;
    ctx.flags = 0;
    ctx.maxChars = 0;
    ctx.totalChars = 0;
    ctx.currentChars = 0;
    ctx.buffer = NULL;
    ctx.width = 0;
    ctx.precision = EOF;
    ctx.stream = stream;

    _PLATFORM_MUTEX_ACQUIRE(&stream->mtx);

    if (_FilePrepareRead(stream) == ERR || _FILE_CHECK_AVAIL(stream) == ERR)
    {
        _PLATFORM_MUTEX_RELEASE(&stream->mtx);
        return EOF;
    }

    va_copy(ctx.arg, arg);

    while (*format != '\0')
    {
        const char* rc;

        if ((*format != '%') || ((rc = _Scan(format, &ctx)) == format))
        {
            int c;

            /* No conversion specifier, match verbatim */
            if (isspace((unsigned char)*format))
            {
                /* Whitespace char in format string: Skip all whitespaces */
                /* No whitespaces in input does not result in matching error */
                while (isspace((unsigned char)(c = getc(stream))))
                {
                    ++ctx.totalChars;
                }

                if (!feof(stream))
                {
                    ungetc(c, stream);
                }
            }
            else
            {
                /* Non-whitespace char in format string: Match verbatim */
                if (((c = getc(stream)) != *format) || feof(stream))
                {
                    /* Matching error */
                    if (!feof(stream) && !ferror(stream))
                    {
                        ungetc(c, stream);
                    }
                    else if (ctx.maxChars == 0)
                    {
                        _PLATFORM_MUTEX_RELEASE(&stream->mtx);
                        return EOF;
                    }

                    _PLATFORM_MUTEX_RELEASE(&stream->mtx);
                    return ctx.maxChars;
                }
                else
                {
                    ++ctx.totalChars;
                }
            }

            ++format;
        }
        else
        {
            /* NULL return code indicates matching error */
            if (rc == NULL)
            {
                break;
            }

            /* Continue parsing after conversion specifier */
            format = rc;
        }
    }

    va_end(ctx.arg);
    _PLATFORM_MUTEX_RELEASE(&stream->mtx);
    return ctx.maxChars;
}