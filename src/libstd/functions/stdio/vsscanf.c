#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

#include "common/scan.h"

int vsscanf(const char* _RESTRICT s, const char* _RESTRICT format, va_list arg)
{
    /* TODO: This function should interpret format as multibyte characters.  */
    _format_ctx_t ctx;
    ctx.base = 0;
    ctx.flags = 0;
    ctx.maxChars = 0;
    ctx.totalChars = 0;
    ctx.currentChars = 0;
    ctx.buffer = (char*)s;
    ctx.width = 0;
    ctx.precision = EOF;
    ctx.stream = NULL;
    va_copy(ctx.arg, arg);

    while (*format != '\0')
    {
        const char* rc;

        if ((*format != '%') || ((rc = _scan(format, &ctx)) == format))
        {
            /* No conversion specifier, match verbatim */
            if (isspace((unsigned char)*format))
            {
                /* Whitespace char in format string: Skip all whitespaces */
                /* No whitespaces in input do not result in matching error */
                while (isspace((unsigned char)*ctx.buffer))
                {
                    ++ctx.buffer;
                    ++ctx.totalChars;
                }
            }
            else
            {
                /* Non-whitespace char in format string: Match verbatim */
                if (*ctx.buffer != *format)
                {
                    if (*ctx.buffer == '\0' && ctx.maxChars == 0)
                    {
                        /* Early input error */
                        return EOF;
                    }

                    /* Matching error */
                    return ctx.maxChars;
                }
                else
                {
                    ++ctx.buffer;
                    ++ctx.totalChars;
                }
            }

            ++format;
        }
        else
        {
            /* NULL return code indicates error */
            if (rc == NULL)
            {
                if ((*ctx.buffer == '\n') && (ctx.maxChars == 0))
                {
                    ctx.maxChars = EOF;
                }

                break;
            }

            /* Continue parsing after conversion specifier */
            format = rc;
        }
    }

    va_end(ctx.arg);
    return ctx.maxChars;
}