#pragma once

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/defs.h>

#include "digits.h"

#ifndef _SCAN_GET
#error "_SCAN_GET not defined"
#endif

#ifndef _SCAN_UNGET
#error "_SCAN_UNGET not defined"
#endif

/**
 * @brief Internal Scan Implementation.
 * @defgroup libstd_common_scan Scan
 * @ingroup libstd_common
 *
 * Provides a common implementation for scanning formatted input, any function that needs to scan formatted input should
 * define the `_SCAN_GET()` and `_SCAN_UNGET()` macros before including this file.
 *
 * The `_SCAN_GET(ctx)` macro should evaluate to an expression that returns the next character from the input source and
 * takes a pointer to the current `scan_ctx_t` as an argument.
 *
 * The`_SCAN_UNGET(ctx, c)` macro should evaluate to an expression that pushes back the character `c` to the input
 * source and takes a pointer to the current `scan_ctx_t` and the character to push back as argument.
 *
 * @todo Implement floating point scanning.
 *
 * @see https://cplusplus.com/reference/cstdio/scanf/ for details on the format specifiers.
 *
 * @{
 */

typedef struct
{
    int parsedItems;
    const char* p;
    va_list arg;
    void* data;
    uint64_t count;
    char prev;
} _scan_ctx_t;

static inline int _scan_next(_scan_ctx_t* ctx)
{
    if (ctx->prev != EOF)
    {
        int c = ctx->prev;
        ctx->prev = EOF;
        ctx->count++;
        return c;
    }

    int c = _SCAN_GET(ctx);
    if (c != EOF)
    {
        ctx->count++;
    }
    return c;
}

static inline void _scan_undo(_scan_ctx_t* ctx, int c)
{
    if (c != EOF)
    {
        assert(ctx->count > 0);
        assert(ctx->prev == EOF);
        ctx->prev = (char)c;
        ctx->count--;
    }
}

static inline int _scan_whitespace(_scan_ctx_t* ctx)
{
    while (isspace(*ctx->p))
    {
        ctx->p++;
    }

    int c;
    while (true)
    {
        c = _scan_next(ctx);
        if (c == EOF)
        {
            return EOF;
        }

        if (!isspace(c))
        {
            _scan_undo(ctx, c);
            break;
        }
    }

    return 0;
}

typedef enum
{
    _SCAN_SUPPRESS_ASSIGNMENT = 1 << 0,
} _scan_format_flags_t;

typedef enum
{
    _SCAN_DEFAULT = 0,
    _SCAN_HH,
    _SCAN_H,
    _SCAN_L,
    _SCAN_LL,
    _SCAN_J,
    _SCAN_Z,
    _SCAN_T,
} _scan_format_length_t;

typedef struct
{
    _scan_format_flags_t flags;
    uint64_t width;
    _scan_format_length_t length;
} _scan_format_ctx_t;

static inline int _scan_asign_signed_int(_scan_ctx_t* ctx, _scan_format_ctx_t* format, int64_t value)
{
    switch (format->length)
    {
    case _SCAN_DEFAULT:
        *va_arg(ctx->arg, int*) = (int)value;
        break;
    case _SCAN_HH:
        *va_arg(ctx->arg, signed char*) = (signed char)value;
        break;
    case _SCAN_H:
        *va_arg(ctx->arg, short int*) = (short int)value;
        break;
    case _SCAN_L:
        *va_arg(ctx->arg, long int*) = (long int)value;
        break;
    case _SCAN_LL:
        *va_arg(ctx->arg, long long int*) = (long long int)value;
        break;
    case _SCAN_J:
        *va_arg(ctx->arg, intmax_t*) = (intmax_t)value;
        break;
    case _SCAN_Z:
        *va_arg(ctx->arg, size_t*) = (size_t)value;
        break;
    case _SCAN_T:
        *va_arg(ctx->arg, ptrdiff_t*) = (ptrdiff_t)value;
        break;
    default:
        return EOF;
    }

    return 0;
}

static inline int _scan_asign_unsigned_int(_scan_ctx_t* ctx, _scan_format_ctx_t* format, uint64_t value)
{
    switch (format->length)
    {
    case _SCAN_DEFAULT:
        *va_arg(ctx->arg, unsigned int*) = (unsigned int)value;
        break;
    case _SCAN_HH:
        *va_arg(ctx->arg, unsigned char*) = (unsigned char)value;
        break;
    case _SCAN_H:
        *va_arg(ctx->arg, unsigned short int*) = (unsigned short int)value;
        break;
    case _SCAN_L:
        *va_arg(ctx->arg, unsigned long int*) = (unsigned long int)value;
        break;
    case _SCAN_LL:
        *va_arg(ctx->arg, unsigned long long int*) = (unsigned long long int)value;
        break;
    case _SCAN_J:
        *va_arg(ctx->arg, uintmax_t*) = (uintmax_t)value;
        break;
    case _SCAN_Z:
        *va_arg(ctx->arg, size_t*) = (size_t)value;
        break;
    case _SCAN_T:
        *va_arg(ctx->arg, ptrdiff_t*) = (ptrdiff_t)value;
        break;
    default:
        return EOF;
    }

    return 0;
}

typedef enum
{
    _SCAN_INTEGER_UNSIGNED = 0,
    _SCAN_INTEGER_SIGNED = 1 << 0,
} _scan_format_integer_flags_t;

static inline int _scan_format_integer(_scan_ctx_t* ctx, _scan_format_ctx_t* format, uint32_t base,
    _scan_format_integer_flags_t flags)
{
    int sign = 1;

    int c = _scan_next(ctx);
    if (c == EOF)
    {
        return EOF;
    }

    if (c == '-')
    {
        sign = -1;
        c = _scan_next(ctx);
        if (c == EOF)
        {
            return EOF;
        }
    }
    else if (c == '+')
    {
        c = _scan_next(ctx);
        if (c == EOF)
        {
            return EOF;
        }
    }

    if (base == 0 || base == 16)
    {
        if (c == '0')
        {
            int next = _scan_next(ctx);
            if ((next == 'x' || next == 'X') && (base == 0 || base == 16))
            {
                base = 16;
                c = _scan_next(ctx);
            }
            else
            {
                if (base == 0)
                {
                    base = 8;
                }

                if (next != EOF)
                {
                    _scan_undo(ctx, next);
                }
            }
        }
    }

    if (base == 0)
    {
        base = 10;
    }

    uint64_t value = 0;
    uint64_t digits = 0;
    while (c != EOF && digits < format->width)
    {
        uint8_t digit = _digit_to_int(c);
        if (digit >= base)
        {
            _scan_undo(ctx, c);
            break;
        }

        value = (value * base) + digit;
        c = _scan_next(ctx);
        digits++;
    }

    if (digits == 0)
    {
        return 0;
    }

    if (format->flags & _SCAN_SUPPRESS_ASSIGNMENT)
    {
        return 0;
    }

    if (flags & _SCAN_INTEGER_SIGNED)
    {
        int64_t signedValue = (int64_t)value * sign;
        if (_scan_asign_signed_int(ctx, format, signedValue) == EOF)
        {
            return EOF;
        }

        ctx->parsedItems++;
        return 0;
    }

    if (_scan_asign_unsigned_int(ctx, format, value) == EOF)
    {
        return EOF;
    }

    ctx->parsedItems++;
    return 0;
}

static inline int _scan_format_float(_scan_ctx_t* ctx, _scan_format_ctx_t* format)
{
    /// @todo Implement floating point scanning
    (void)ctx;
    (void)format;
    return EOF;
}

static inline int _scan_format_char(_scan_ctx_t* ctx, _scan_format_ctx_t* format)
{
    uint64_t width = format->width == UINT64_MAX ? 1 : format->width;

    if (format->flags & _SCAN_SUPPRESS_ASSIGNMENT)
    {
        for (uint64_t i = 0; i < width; i++)
        {
            int c = _scan_next(ctx);
            if (c == EOF)
            {
                return EOF;
            }
        }

        return 0;
    }

    ctx->parsedItems++;

    char* buffer = va_arg(ctx->arg, char*);
    for (uint64_t i = 0; i < width; i++)
    {
        int c = _scan_next(ctx);
        if (c == EOF)
        {
            return EOF;
        }

        buffer[i] = (char)c;
    }

    return 0;
}

static inline int _scan_format_string(_scan_ctx_t* ctx, _scan_format_ctx_t* format)
{
    char* buffer = NULL;
    if (!(format->flags & _SCAN_SUPPRESS_ASSIGNMENT))
    {
        buffer = va_arg(ctx->arg, char*);
    }

    uint64_t count = 0;
    while (count < format->width)
    {
        int c = _scan_next(ctx);
        if (c == EOF || isspace(c))
        {
            if (c != EOF)
            {
                _scan_undo(ctx, c);
            }
            break;
        }

        if (buffer != NULL)
        {
            buffer[count] = (char)c;
        }

        count++;
    }

    if (count == 0)
    {
        return EOF;
    }

    if (buffer != NULL)
    {
        buffer[count] = '\0';
        ctx->parsedItems++;
    }

    return 0;
}

typedef struct
{
    uint64_t table[UINT8_MAX / (sizeof(uint64_t) * 8) + 1];
    bool invert;
} _scanset_t;

static inline void _scanset_set(_scanset_t* scanset, uint8_t c)
{
    scanset->table[c / (sizeof(uint64_t) * 8)] |= ((uint64_t)1 << (c % (sizeof(uint64_t) * 8)));
}

static inline bool _scanset_get(_scanset_t* scanset, uint8_t c)
{
    bool found = (scanset->table[c / (sizeof(uint64_t) * 8)] & ((uint64_t)1 << (c % (sizeof(uint64_t) * 8)))) != 0;
    return found ^ scanset->invert;
}

static inline int _scan_format_scanset(_scan_ctx_t* ctx, _scan_format_ctx_t* format)
{
    _scanset_t scanset = {0};

    if (*ctx->p == '^')
    {
        scanset.invert = true;
        ctx->p++;
    }

    if (*ctx->p == ']')
    {
        _scanset_set(&scanset, *ctx->p);
        ctx->p++;
    }

    while (*ctx->p != '\0' && *ctx->p != ']')
    {
        if (ctx->p[1] == '-' && ctx->p[2] != ']' && ctx->p[2] != '\0')
        {
            unsigned char start = (unsigned char)*ctx->p;
            unsigned char end = (unsigned char)ctx->p[2];

            if (start <= end)
            {
                for (int i = start; i <= end; i++)
                {
                    _scanset_set(&scanset, (uint8_t)i);
                }
            }
            else
            {
                _scanset_set(&scanset, start);
                _scanset_set(&scanset, '-');
                _scanset_set(&scanset, end);
            }

            ctx->p += 3;
        }
        else
        {
            _scanset_set(&scanset, *ctx->p);
            ctx->p++;
        }
    }

    if (*ctx->p == ']')
    {
        ctx->p++;
    }

    char* buffer = NULL;
    if (!(format->flags & _SCAN_SUPPRESS_ASSIGNMENT))
    {
        /// @todo Wide chars
        buffer = va_arg(ctx->arg, char*);
    }

    uint64_t count = 0;
    while (count < format->width)
    {
        int c = _scan_next(ctx);
        if (c == EOF)
        {
            return EOF;
        }

        if (_scanset_get(&scanset, (uint8_t)c))
        {
            if (buffer != NULL)
            {
                buffer[count] = (char)c;
            }

            count++;
        }
        else
        {
            _scan_undo(ctx, c);
            break;
        }
    }

    if (count == 0)
    {
        return EOF;
    }

    if (buffer != NULL)
    {
        buffer[count] = '\0';
        ctx->parsedItems++;
    }

    return 0;
}

static inline int _scan_format_count(_scan_ctx_t* ctx, _scan_format_ctx_t* format)
{
    if (format->flags & _SCAN_SUPPRESS_ASSIGNMENT)
    {
        return 0;
    }

    return _scan_asign_signed_int(ctx, format, ctx->count);
}

static inline int _scan_format_percent(_scan_ctx_t* ctx, _scan_format_ctx_t* format)
{
    (void)format;

    int c = _scan_next(ctx);
    if (c == EOF)
    {
        return EOF;
    }

    if (c != '%')
    {
        _scan_undo(ctx, c);
        return EOF;
    }

    return 0;
}

static inline int _scan_format(_scan_ctx_t* ctx)
{
    // %[*][width][length]specifier

    _scan_format_ctx_t format = {
        .flags = 0,
        .width = 0,
        .length = _SCAN_DEFAULT,
    };

    if (*ctx->p == '*')
    {
        format.flags |= _SCAN_SUPPRESS_ASSIGNMENT;
        ctx->p++;
    }

    while (isdigit(*ctx->p))
    {
        format.width = format.width * 10 + (*ctx->p - '0');
        ctx->p++;
    }

    if (format.width == 0)
    {
        format.width = UINT64_MAX;
    }

    switch (*ctx->p)
    {
    case 'h':
        ctx->p++;
        if (*ctx->p == 'h')
        {
            format.length = _SCAN_HH;
            ctx->p++;
        }
        else
        {
            format.length = _SCAN_H;
        }
        break;
    case 'l':
        ctx->p++;
        if (*ctx->p == 'l')
        {
            format.length = _SCAN_LL;
            ctx->p++;
        }
        else
        {
            format.length = _SCAN_L;
        }
        break;
    case 'j':
        format.length = _SCAN_J;
        ctx->p++;
        break;
    case 'z':
        format.length = _SCAN_Z;
        ctx->p++;
        break;
    case 't':
        format.length = _SCAN_T;
        ctx->p++;
        break;
    default:
        break;
    }

    char specifier = *ctx->p;
    ctx->p++;

    int ret = 0;
    switch (specifier)
    {
    case 'i':
        ret = _scan_format_integer(ctx, &format, 0, _SCAN_INTEGER_SIGNED);
        break;
    case 'd':
        ret = _scan_format_integer(ctx, &format, 10, _SCAN_INTEGER_SIGNED);
        break;
    case 'u':
        ret = _scan_format_integer(ctx, &format, 10, _SCAN_INTEGER_UNSIGNED);
        break;
    case 'o':
        ret = _scan_format_integer(ctx, &format, 8, _SCAN_INTEGER_UNSIGNED);
        break;
    case 'x':
        ret = _scan_format_integer(ctx, &format, 16, _SCAN_INTEGER_UNSIGNED);
        break;
    case 'f':
    case 'e':
    case 'g':
    case 'a':
        ret = _scan_format_float(ctx, &format);
        break;
    case 'c':
        ret = _scan_format_char(ctx, &format);
        break;
    case 's':
        ret = _scan_format_string(ctx, &format);
        break;
    case 'p':
        format.length = _SCAN_Z;
        ret = _scan_format_integer(ctx, &format, 16, _SCAN_INTEGER_UNSIGNED);
        break;
    case '[':
        ret = _scan_format_scanset(ctx, &format);
        break;
    case 'n':
        ret = _scan_format_count(ctx, &format);
        break;
    case '%':
        ret = _scan_format_percent(ctx, &format);
        break;
    default:
        ret = EOF;
        break;
    }

    return ret;
}

static inline int _scan(const char* _RESTRICT format, va_list arg, void* data)
{
    assert(format != NULL);

    _scan_ctx_t ctx = {
        .parsedItems = 0,
        .p = format,
        .data = data,
        .count = 0,
        .prev = EOF,
    };
    va_copy(ctx.arg, arg);

    while (*ctx.p != '\0')
    {
        if (isspace(*ctx.p))
        {
            ctx.p++;
            if (_scan_whitespace(&ctx) == EOF)
            {
                break;
            }

            continue;
        }

        if (*ctx.p == '%')
        {
            ctx.p++;
            if (_scan_format(&ctx) == EOF)
            {
                break;
            }

            continue;
        }

        int c = _scan_next(&ctx);
        if (c == EOF)
        {
            break;
        }

        if (c != *ctx.p)
        {
            _scan_undo(&ctx, c);
            break;
        }

        ctx.p++;
    }

    va_end(ctx.arg);

    if (ctx.prev != EOF)
    {
        _SCAN_UNGET(&ctx, ctx.prev);
        ctx.prev = EOF;
    }

    if (ctx.count == 0)
    {
        return EOF;
    }

    return ctx.parsedItems;
}

/** @} */