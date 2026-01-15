#pragma once

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/defs.h>

#include "digits.h"

#ifndef _PRINT_WRITE
#error "_PRINT_WRITE not defined"
#endif

#ifndef _PRINT_FILL
#error "_PRINT_FILL not defined"
#endif

/**
 * @brief Internal Print Implementation.
 * @defgroup libstd_common_print Print
 * @ingroup libstd_common
 *
 * Provides a common implementation for printing formatted output, any function that needs to print formatted output
 * should define the `_PRINT_WRITE()` and `_PRINT_FILL()` macros before including this file.
 *
 * The `_PRINT_WRITE(ctx, buffer, count)` macro should evaluate to an expression that writes `count` bytes from the
 * `buffer` to the output source and takes a pointer to the current `print_ctx_t`, a pointer to the buffer and the
 * number of bytes to write as arguments.
 *
 * The `_PRINT_FILL(ctx, c, count)` macro should evaluate to an expression that writes `count` bytes of character `c` to
 * the output source and takes a pointer to the current `print_ctx_t`, the character to write and the number of times to
 * write it as arguments.
 *
 * The macros should return the number of bytes written, or `EOF` on error.
 *
 * @todo Implement floating point printing.
 *
 * @see https://cplusplus.com/reference/cstdio/printf/ for details on the format specifiers.
 *
 * @{
 */

typedef struct
{
    size_t written;
    size_t n;
    const char* p;
    va_list arg;
    void* private;
} _print_ctx_t;
typedef enum
{
    _PRINT_LEFT_ALIGNED = 1 << 1,
    _PRINT_FORCE_SIGN = 1 << 2,
    _PRINT_SPACE_SIGN = 1 << 3,
    _PRINT_ALTERNATE_FORM = 1 << 4,
    _PRINT_UPPER_CASE = 1 << 5,
    _PRINT_PAD_ZERO = 1 << 6,
} _print_format_flags_t;

typedef enum
{
    _PRINT_DEFAULT = 0,
    _PRINT_HH,
    _PRINT_H,
    _PRINT_L,
    _PRINT_LL,
    _PRINT_J,
    _PRINT_Z,
    _PRINT_T,
} _print_format_length_t;

typedef struct
{
    _print_format_flags_t flags;
    int width;
    int precision;
    _print_format_length_t length;
} _print_format_ctx_t;

static inline int _print_padding_left(_print_ctx_t* ctx, _print_format_ctx_t* format, int len)
{
    int padding = 0;
    if (format->width > len)
    {
        padding = format->width - len;
    }

    if (!(format->flags & _PRINT_LEFT_ALIGNED))
    {
        char paddingChar = ' ';
        if (format->flags & _PRINT_PAD_ZERO && format->precision == EOF)
        {
            paddingChar = '0';
        }

        if (_PRINT_FILL(ctx, paddingChar, padding) == EOF)
        {
            return EOF;
        }
        ctx->written += padding;
    }

    return padding;
}

static inline int _print_padding_right(_print_ctx_t* ctx, _print_format_ctx_t* format, int padding)
{
    if (format->flags & _PRINT_LEFT_ALIGNED)
    {
        if (_PRINT_FILL(ctx, ' ', padding) == EOF)
        {
            return EOF;
        }
        ctx->written += padding;
    }

    return 0;
}

typedef struct
{
    char prefix[20];
    char* prefixPtr;
    char data[32];
    char* dataPtr;
    int8_t sign;
    int8_t base;
} _print_integer_t;

#define _PRINT_INTEGER_PUSH(integer, c) (*--(integer)->dataPtr = (c))

#define _PRINT_INTEGER_PREFIX_PUSH(integer, c) (*--(integer)->prefixPtr = (c))

#define _PRINT_INTEGER_LEN(integer) ((int)((sizeof((integer)->data)) - ((integer)->dataPtr - (integer)->data)))

#define _PRINT_INTEGER_PREFIX_LEN(integer) \
    ((int)((sizeof((integer)->prefix)) - ((integer)->prefixPtr - (integer)->prefix)))

static inline int _print_integer_print(_print_ctx_t* ctx, _print_format_ctx_t* format, _print_integer_t* integer)
{
    int integerLen = _PRINT_INTEGER_LEN(integer);

    if (format->flags & _PRINT_ALTERNATE_FORM && integerLen > 0)
    {
        if (integer->base == 8)
        {
            _PRINT_INTEGER_PREFIX_PUSH(integer, '0');
        }
        else if (integer->base == 16)
        {
            _PRINT_INTEGER_PREFIX_PUSH(integer, format->flags & _PRINT_UPPER_CASE ? 'X' : 'x');
            _PRINT_INTEGER_PREFIX_PUSH(integer, '0');
        }
    }

    int precision = 0;
    if (format->precision == EOF)
    {
        if (integerLen == 0)
        {
            _PRINT_INTEGER_PUSH(integer, '0');
            integerLen++;
        }
    }
    else if (integerLen < format->precision)
    {
        precision = format->precision - integerLen;
    }

    if (integer->sign < 0)
    {
        _PRINT_INTEGER_PREFIX_PUSH(integer, '-');
    }
    else if (integer->sign > 0)
    {
        if (format->flags & _PRINT_FORCE_SIGN)
        {
            _PRINT_INTEGER_PREFIX_PUSH(integer, '+');
        }
        else if (format->flags & _PRINT_SPACE_SIGN)
        {
            _PRINT_INTEGER_PREFIX_PUSH(integer, ' ');
        }
    }

    int prefixLen = _PRINT_INTEGER_PREFIX_LEN(integer);

    bool padZeroes = (format->flags & _PRINT_PAD_ZERO) && (format->precision == EOF);
    if (prefixLen > 0 && padZeroes)
    {
        if (_PRINT_WRITE(ctx, integer->prefixPtr, prefixLen) == EOF)
        {
            return EOF;
        }
        ctx->written += prefixLen;
    }

    int padding = _print_padding_left(ctx, format, integerLen + prefixLen + precision);
    if (padding < 0)
    {
        return EOF;
    }

    if (prefixLen > 0 && !padZeroes)
    {
        if (_PRINT_WRITE(ctx, integer->prefixPtr, prefixLen) == EOF)
        {
            return EOF;
        }
        ctx->written += prefixLen;
    }

    if (_PRINT_FILL(ctx, '0', precision) == EOF)
    {
        return EOF;
    }
    ctx->written += precision;

    if (_PRINT_WRITE(ctx, integer->dataPtr, integerLen) == EOF)
    {
        return EOF;
    }
    ctx->written += integerLen;

    return _print_padding_right(ctx, format, padding);
}

static inline int _print_format_signed_integer(_print_ctx_t* ctx, _print_format_ctx_t* format)
{
    intmax_t value = 0;
    switch (format->length)
    {
    case _PRINT_DEFAULT:
        value = va_arg(ctx->arg, int);
        break;
    case _PRINT_HH:
        value = (signed char)va_arg(ctx->arg, int);
        break;
    case _PRINT_H:
        value = (short int)va_arg(ctx->arg, int);
        break;
    case _PRINT_L:
        value = va_arg(ctx->arg, long int);
        break;
    case _PRINT_LL:
        value = va_arg(ctx->arg, long long int);
        break;
    case _PRINT_J:
        value = va_arg(ctx->arg, intmax_t);
        break;
    case _PRINT_Z:
        value = va_arg(ctx->arg, size_t);
        break;
    case _PRINT_T:
        value = va_arg(ctx->arg, ptrdiff_t);
        break;
    default:
        return EOF;
    }

    _print_integer_t integer = {.prefixPtr = integer.prefix + sizeof(integer.prefix),
        .dataPtr = integer.data + sizeof(integer.data),
        .base = 10,
        .sign = 1};

    uintmax_t uvalue = (uintmax_t)value;
    if (value < 0)
    {
        integer.sign = -1;
        uvalue = (uintmax_t)(-value);
    }

    while (uvalue >= 100)
    {
        unsigned index = (uvalue % 100) * 2;
        uvalue /= 100;
        _PRINT_INTEGER_PUSH(&integer, _digitPairs[index + 1]);
        _PRINT_INTEGER_PUSH(&integer, _digitPairs[index]);
    }

    while (uvalue > 0)
    {
        _PRINT_INTEGER_PUSH(&integer, _int_to_digit(uvalue % 10));
        uvalue /= 10;
    }

    return _print_integer_print(ctx, format, &integer);
}

static inline int _print_format_unsigned_integer(_print_ctx_t* ctx, _print_format_ctx_t* format, uint32_t base)
{
    uintmax_t value = 0;
    switch (format->length)
    {
    case _PRINT_DEFAULT:
        value = va_arg(ctx->arg, unsigned int);
        break;
    case _PRINT_HH:
        value = (unsigned char)va_arg(ctx->arg, unsigned int);
        break;
    case _PRINT_H:
        value = (unsigned short int)va_arg(ctx->arg, unsigned int);
        break;
    case _PRINT_L:
        value = va_arg(ctx->arg, unsigned long int);
        break;
    case _PRINT_LL:
        value = va_arg(ctx->arg, unsigned long long int);
        break;
    case _PRINT_J:
        value = va_arg(ctx->arg, uintmax_t);
        break;
    case _PRINT_Z:
        value = va_arg(ctx->arg, size_t);
        break;
    case _PRINT_T:
        value = va_arg(ctx->arg, ptrdiff_t);
        break;
    default:
        return EOF;
    }

    _print_integer_t integer = {.prefixPtr = integer.prefix + sizeof(integer.prefix),
        .dataPtr = integer.data + sizeof(integer.data),
        .base = base,
        .sign = 0};

    if (base == 10)
    {
        while (value >= 100)
        {
            unsigned int index = (value % 100) * 2;
            value /= 100;
            _PRINT_INTEGER_PUSH(&integer, _digitPairs[index + 1]);
            _PRINT_INTEGER_PUSH(&integer, _digitPairs[index]);
        }
        while (value > 0)
        {
            _PRINT_INTEGER_PUSH(&integer, _int_to_digit(value % 10));
            value /= 10;
        }
    }
    else if (base == 16)
    {
        const char* digits = (format->flags & _PRINT_UPPER_CASE) ? _Xdigits : _xdigits;
        while (value > 0)
        {
            _PRINT_INTEGER_PUSH(&integer, digits[value & 0xF]);
            value >>= 4;
        }
    }
    else if (base == 8)
    {
        while (value > 0)
        {
            _PRINT_INTEGER_PUSH(&integer, _int_to_digit(value & 7));
            value >>= 3;
        }
    }
    else
    {
        return EOF;
    }

    return _print_integer_print(ctx, format, &integer);
}

static inline int _print_format_char(_print_ctx_t* ctx, _print_format_ctx_t* format)
{
    char c = (char)va_arg(ctx->arg, int);

    int padding = _print_padding_left(ctx, format, 1);
    if (padding < 0)
    {
        return EOF;
    }

    if (_PRINT_WRITE(ctx, &c, 1) == EOF)
    {
        return EOF;
    }
    ctx->written++;

    return _print_padding_right(ctx, format, padding);
}

static inline int _print_format_string(_print_ctx_t* ctx, _print_format_ctx_t* format)
{
    const char* s = va_arg(ctx->arg, const char*);
    if (s == NULL)
    {
        s = "(null)"; // Not standard but very useful
    }

    int len = 0;
    for (int i = 0; s[i] != '\0' && (format->precision == EOF || i < format->precision); i++)
    {
        len++;
    }

    int padding = _print_padding_left(ctx, format, len);
    if (padding < 0)
    {
        return EOF;
    }

    if (_PRINT_WRITE(ctx, s, len) == EOF)
    {
        return EOF;
    }
    ctx->written += len;

    return _print_padding_right(ctx, format, padding);
}

static inline int _print_format_written(_print_ctx_t* ctx)
{
    signed int* written = va_arg(ctx->arg, signed int*);
    *written = (signed int)ctx->written;
    return 0;
}

static inline int _print_format_percent(_print_ctx_t* ctx)
{
    if (_PRINT_WRITE(ctx, "%", 1) == EOF)
    {
        return EOF;
    }
    ctx->written++;
    return 1;
}

static inline int _print_format(_print_ctx_t* ctx)
{
    // %[flags][width][.precision][length]specifier

    _print_format_ctx_t format = {
        .flags = 0,
        .width = EOF,
        .precision = EOF,
        .length = _PRINT_DEFAULT,
    };

    while (true)
    {
        switch (*ctx->p)
        {
        case '-':
            format.flags |= _PRINT_LEFT_ALIGNED;
            ctx->p++;
            break;
        case '+':
            format.flags |= _PRINT_FORCE_SIGN;
            ctx->p++;
            break;
        case ' ':
            format.flags |= _PRINT_SPACE_SIGN;
            ctx->p++;
            break;
        case '#':
            format.flags |= _PRINT_ALTERNATE_FORM;
            ctx->p++;
            break;
        case '0':
            format.flags |= _PRINT_PAD_ZERO;
            ctx->p++;
            break;
        default:
            goto flags_done;
        }
    }
flags_done:

    if (*ctx->p == '*')
    {
        format.width = va_arg(ctx->arg, int);
        ctx->p++;
    }
    else if (isdigit(*ctx->p))
    {
        format.width = *ctx->p - '0';
        ctx->p++;
        while (isdigit(*ctx->p))
        {
            format.width = format.width * 10 + (*ctx->p - '0');
            ctx->p++;
        }
    }

    if (*ctx->p == '.')
    {
        format.precision = 0;
        ctx->p++;
        if (*ctx->p == '*')
        {
            format.precision = va_arg(ctx->arg, int);
            ctx->p++;
        }
        else
        {
            while (isdigit(*ctx->p))
            {
                format.precision = format.precision * 10 + (*ctx->p - '0');
                ctx->p++;
            }
        }
    }

    switch (*ctx->p)
    {
    case 'h':
        ctx->p++;
        if (*ctx->p == 'h')
        {
            format.length = _PRINT_HH;
            ctx->p++;
        }
        else
        {
            format.length = _PRINT_H;
        }
        break;
    case 'l':
        ctx->p++;
        if (*ctx->p == 'l')
        {
            format.length = _PRINT_LL;
            ctx->p++;
        }
        else
        {
            format.length = _PRINT_L;
        }
        break;
    case 'j':
        format.length = _PRINT_J;
        ctx->p++;
        break;
    case 'z':
        format.length = _PRINT_Z;
        ctx->p++;
        break;
    case 't':
        format.length = _PRINT_T;
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
    case 'd':
    case 'i':
        ret = _print_format_signed_integer(ctx, &format);
        break;
    case 'u':
        ret = _print_format_unsigned_integer(ctx, &format, 10);
        break;
    case 'o':
        ret = _print_format_unsigned_integer(ctx, &format, 8);
        break;
    case 'X':
        format.flags |= _PRINT_UPPER_CASE;
    case 'x': // Fallthrough
        ret = _print_format_unsigned_integer(ctx, &format, 16);
        break;
    case 'F':
    case 'f':
    case 'E':
    case 'e':
    case 'G':
    case 'g':
    case 'A':
    case 'a':
        /// @todo Implement floating point formatting
        ret = EOF;
        break;
    case 'c':
        format.flags &= ~_PRINT_PAD_ZERO;
        ret = _print_format_char(ctx, &format);
        break;
    case 's':
        format.flags &= ~_PRINT_PAD_ZERO;
        ret = _print_format_string(ctx, &format);
        break;
    case 'p':
        format.length = _PRINT_Z;
        format.flags |= _PRINT_ALTERNATE_FORM | _PRINT_PAD_ZERO;
        format.precision = 2 * sizeof(void*);
        ret = _print_format_unsigned_integer(ctx, &format, 16);
        break;
    case 'n':
        ret = _print_format_written(ctx);
        break;
    case '%':
        ret = _print_format_percent(ctx);
        break;
    default:
        ret = EOF;
        break;
    }

    return ret;
}

static inline int _print(const char* _RESTRICT format, size_t n, va_list arg, void* private)
{
    assert(format != NULL);

    _print_ctx_t ctx = {
        .written = 0,
        .n = n,
        .p = format,
        .private = private,
    };
    va_copy(ctx.arg, arg);

    while (true)
    {
        uint64_t count = 0;
        while (ctx.p[count] != '%' && ctx.p[count] != '\0')
        {
            count++;
        }

        if (count > 0)
        {
            if (_PRINT_WRITE(&ctx, ctx.p, count) == EOF)
            {
                break;
            }
            ctx.written += count;
            ctx.p += count;
        }

        if (*ctx.p == '\0')
        {
            break;
        }
        ctx.p++;

        if (_print_format(&ctx) == EOF)
        {
            break;
        }
    }

    va_end(ctx.arg);
    return ctx.written;
}

/** @} */