#include <stdio.h>

#include "common/print.h"

int vsnprintf(char* _RESTRICT s, size_t n, const char* _RESTRICT format, va_list arg)
{
    _format_ctx_t ctx;
    ctx.base = 0;
    ctx.flags = 0;
    ctx.maxChars = n;
    ctx.totalChars = 0;
    ctx.currentChars = 0;
    ctx.buffer = s;
    ctx.width = 0;
    ctx.precision = EOF;
    ctx.stream = NULL;
    va_copy(ctx.arg, arg);

    while (*format != '\0')
    {
        const char* rc;

        if ((*format != '%') || ((rc = _print(format, &ctx)) == format))
        {
            if (ctx.totalChars < n)
            {
                s[ctx.totalChars] = *format;
            }

            ctx.totalChars++;
            format++;
        }
        else
        {
            /* Continue parsing after conversion specifier */
            format = rc;
        }
    }

    if (ctx.totalChars < n)
    {
        s[ctx.totalChars] = '\0';
    }

    va_end(ctx.arg);
    return ctx.totalChars;
}